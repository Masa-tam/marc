#include "frame/lzss_position_distance_1m_finder_scratch_frame.hpp"
#include "frame/lzss_position_distance_1m_prepared_owned_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_1m_finder_scratch_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_1m_finder_scratch_owned_encoder.hpp"
#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Encoder=LzssPositionDistance1mFinderScratchFrameStreamingEncoder;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
using Search=marc::dictionary::internal::LzssPositionDistance1mSearch;
constexpr auto end_flag=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush_flag=marc::core::flag_value(marc::core::ProcessFlags::flush);
TypedContextStreamHeader stream_for(std::size_t size,std::uint32_t frame=21) {
    return {frame,size,{1048576,3,258,0},32768,44,9,1,10};
}
struct Storage {
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements requirements{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> aligned;
    explicit Storage(TypedContextStreamHeader stream,Search search=Search::indexed) {
        limits.max_block_size=stream.frame_size;
        EXPECT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),requirements,search),
            LzssPositionDistanceWorkspaceError::none);
        raw.resize(requirements.raw_bytes); serialized.resize(requirements.serialized_bytes);
        aligned.resize((requirements.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        limits.max_internal_buffered_bytes=requirements.aggregate_bytes;
    }
    std::span<std::byte> views() { return std::as_writable_bytes(std::span{aligned}).first(requirements.views_bytes); }
};
std::vector<std::byte> oracle(std::span<const std::byte> input,TypedContextStreamHeader stream,
    unsigned eligibility,Search search) {
    Storage s(stream,search); LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),s.raw,s.serialized,s.views(),v,search),
        LzssPositionDistanceWorkspaceError::none);
    const auto frames=input.size()/stream.frame_size+(input.size()%stream.frame_size!=0);
    std::vector<std::byte> result(112+frames*s.requirements.serialized_bytes);
    std::array<std::byte,112> header{};
    EXPECT_TRUE(serialize_lzss_position_distance_1m_stream_header(stream,s.limits,header));
    std::copy(header.begin(),header.end(),result.begin());
    std::size_t written=112,position=0;std::uint64_t sequence=0;
    while(position<input.size()) {
        const auto count=std::min<std::size_t>(stream.frame_size,input.size()-position);
        const auto frame=encode_lzss_position_distance_1m_raw_frame(stream,s.limits,sequence++,position,
            input.subspan(position,count),eligibility,search,v.tokens,v.operations,v.finder,v.serialized);
        EXPECT_EQ(frame.error,LzssPositionDistanceRawFrameError::none);
        std::copy_n(v.serialized.begin(),frame.frame.serialized_size,result.begin()+written);
        written+=frame.frame.serialized_size;position+=count;
    }
    result.resize(written); return result;
}
std::vector<std::byte> run(std::span<const std::byte> input,TypedContextStreamHeader stream,
    unsigned eligibility,Search search,std::size_t in_chunk,std::size_t out_chunk,bool random=false) {
    Storage s(stream,search);
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views(),eligibility,search);
    std::vector<std::byte> encoded,buffer(out_chunk);
    std::size_t position{}; std::uint32_t seed=17;
    const auto frames=input.size()/stream.frame_size+(input.size()%stream.frame_size!=0);
    for (std::size_t calls=0;calls<1000000;++calls) {
        seed=seed*1664525U+1013904223U;
        auto extent=std::min(input.size()-position,random ? 1+seed%in_chunk : in_chunk);
        auto capacity=calls%7==0 ? 0 : random ? 1+(seed>>16)%out_chunk : out_chunk;
        const auto supplied=input.subspan(position,extent);
        const auto flags=(position+extent==input.size()?end_flag:0)|(calls%2?flush_flag:0);
        const auto result=encoder.process(supplied,std::span{buffer}.first(capacity),flags);
        EXPECT_TRUE(marc::core::is_valid(result,supplied.size(),capacity));
        if(result.status==Status::error) { ADD_FAILURE()<<static_cast<unsigned>(result.error.code); return {}; }
        position+=result.input_consumed;
        encoded.insert(encoded.end(),buffer.begin(),buffer.begin()+result.output_produced);
        const auto full=position/stream.frame_size;
        const auto expected=full+((position==input.size() && position%stream.frame_size!=0)?1:0);
        EXPECT_EQ(encoder.frame_preparation_count(),expected);
        if(result.status==Status::end_of_stream) {
            EXPECT_EQ(position,input.size()); EXPECT_EQ(encoder.frame_preparation_count(),frames);
            const auto ended=encoder.process({}, {},end_flag);
            EXPECT_EQ(ended.status,Status::end_of_stream); EXPECT_EQ(ended.output_produced,0);
            return encoded;
        }
    }
    ADD_FAILURE()<<"incremental encoder did not terminate"; return {};
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, MatchesOracleAcrossPoliciesAndFrameBoundaries) {
    for (std::uint32_t frame:{1U,3U,21U,257U})
    for (std::size_t size:{std::size_t{0},std::size_t{1},std::size_t{frame-1},std::size_t{frame},std::size_t{frame+1},std::size_t{2*frame+7}})
    for(unsigned policy=3;policy<=5;++policy) {
        SCOPED_TRACE(frame);
        SCOPED_TRACE(size);
        SCOPED_TRACE(policy);
        std::vector<std::byte> input(size);
        for(std::size_t i=0;i<size;++i) input[i]=std::byte((i/17)%2 ? i%7 : (i*71+i/11)%256);
        const auto stream=stream_for(size,frame);
        const auto reference=oracle(input,stream,policy,Search::exhaustive);
        EXPECT_EQ(oracle(input,stream,policy,Search::indexed),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed,1,1),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed_five_prefix,1,1),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed_five_prefix,79,31,true),reference);
        EXPECT_EQ(run(input,stream,policy,Search::exhaustive,13,7),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed,79,31,true),reference);
    }
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, EverySmallInputChunkAndMaximumFrame) {
    std::vector<std::byte> input(49,std::byte{'a'});
    const auto stream=stream_for(input.size()); const auto expected=oracle(input,stream,3,Search::indexed);
    for(std::size_t chunk=1;chunk<=input.size();++chunk)
        EXPECT_EQ(run(input,stream,3,Search::indexed,chunk,7),expected);
    input.resize(1048577);
    for(std::size_t i=0;i<input.size();++i) input[i]=std::byte(i%251);
    EXPECT_EQ(run(input,stream_for(input.size(),1048576),3,Search::indexed,8191,4093,true),
        oracle(input,stream_for(input.size(),1048576),3,Search::indexed));
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FlushStarvationAndDelayedEndDoNotPrepareTwice) {
    const auto stream=stream_for(22); Storage s(stream);
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views());
    std::array<std::byte,22> input{}; std::array<std::byte,4096> output{};
    auto result=encoder.process(input,{},end_flag);
    EXPECT_EQ(result.status,Status::need_output); EXPECT_EQ(result.input_consumed,0);
    EXPECT_EQ(encoder.frame_preparation_count(),0);
    result=encoder.process(std::span{input}.first(7),output,flush_flag);
    EXPECT_EQ(result.status,Status::need_input); EXPECT_EQ(result.input_consumed,7);
    EXPECT_EQ(result.output_produced,112); EXPECT_EQ(encoder.frame_preparation_count(),0);
    std::vector<std::byte> encoded(output.begin(),output.begin()+result.output_produced);
    result=encoder.process({},output,flush_flag);
    EXPECT_EQ(result.status,Status::need_input); EXPECT_EQ(result.output_produced,0);
    result=encoder.process(std::span{input}.subspan(7),{},0);
    EXPECT_EQ(result.status,Status::need_output); EXPECT_EQ(result.input_consumed,14);
    EXPECT_EQ(encoder.frame_preparation_count(),1);
    for(int i=0;i<3;++i) {
        result=encoder.process({}, {},flush_flag);
        EXPECT_EQ(result.status,Status::need_output); EXPECT_EQ(encoder.frame_preparation_count(),1);
    }
    result=encoder.process(std::span{input}.last(1),output,0);
    EXPECT_EQ(result.status,Status::need_input); EXPECT_EQ(result.input_consumed,1);
    encoded.insert(encoded.end(),output.begin(),output.begin()+result.output_produced);
    EXPECT_EQ(encoder.frame_preparation_count(),2);
    result=encoder.process({},output,end_flag);
    EXPECT_EQ(result.status,Status::end_of_stream); EXPECT_EQ(result.output_produced,0);
    EXPECT_EQ(encoded,oracle(input,stream,3,Search::indexed));
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, RejectsWrongSizesFlagsAndKeepsErrorsSticky) {
    for(unsigned which=0;which<4;++which) {
        Storage s(stream_for(21)); Encoder encoder(stream_for(21),s.limits,s.raw,s.serialized,s.views());
        std::array<std::byte,22> input{}; std::array<std::byte,112> output{}; output.fill(std::byte{0xcc});
        const auto bytes=which==0?20U:which==1?22U:21U;
        const auto flags=which<2?end_flag:which==2?4U:8U;
        const auto result=encoder.process(std::span{input}.first(bytes),output,flags);
        EXPECT_EQ(result.status,Status::error); EXPECT_EQ(result.input_consumed,0); EXPECT_EQ(result.output_produced,0);
        EXPECT_EQ(result.error.code,which<2?Code::invalid_argument:Code::unsupported);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto x){return x==std::byte{0xcc};}));
        const auto again=encoder.process({},output,0);
        EXPECT_EQ(again.status,Status::error); EXPECT_EQ(again.error.code,result.error.code);
        EXPECT_EQ(again.error.byte_position,result.error.byte_position);
    }
    Storage s(stream_for(1)); Encoder encoder(stream_for(1),s.limits,s.raw,s.serialized,s.views());
    std::array<std::byte,1> input{}; std::array<std::byte,256> output{};
    ASSERT_EQ(encoder.process(input,output,0).status,Status::need_input);
    const auto excess=encoder.process(input,output,0);
    EXPECT_EQ(excess.error.code,Code::invalid_argument); EXPECT_EQ(excess.error.byte_position,1);
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, ConstructorRejectsShortStorageLimitsAndInvalidPolicy) {
    for(unsigned which=0;which<7;++which) {
        Storage s(stream_for(21)); auto raw=std::span{s.raw},serialized=std::span{s.serialized},aligned=s.views();
        unsigned policy=3; auto search=Search::indexed;
        if(which==0) raw=raw.first(raw.size()-1);
        if(which==1) serialized=serialized.first(serialized.size()-1);
        if(which==2) aligned=aligned.first(aligned.size()-1);
        if(which==3) --s.limits.max_internal_buffered_bytes;
        if(which==4) policy=2;
        if(which==5) search=static_cast<Search>(7);
        if(which==6) raw=serialized.first(21);
        Encoder encoder(stream_for(21),s.limits,raw,serialized,aligned,policy,search);
        std::array<std::byte,112> output{}; output.fill(std::byte{0xa5});
        const auto result=encoder.process({},output,0);
        EXPECT_EQ(result.status,Status::error); EXPECT_EQ(result.output_produced,0);
        EXPECT_EQ(result.error.code,which<3?Code::out_of_memory:which==3?Code::limit_exceeded:Code::invalid_argument);
        EXPECT_EQ(encoder.frame_preparation_count(),0);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto x){return x==std::byte{0xa5};}));
    }
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, RejectsProcessOverlapWithEveryLiveRegion) {
    for(unsigned which=0;which<9;++which) {
        Storage s(stream_for(21)); Encoder encoder(stream_for(21),s.limits,s.raw,s.serialized,s.views());
        std::array<std::byte,21> input{}; std::array<std::byte,256> output{};
        std::span<const std::byte> in=input; std::span<std::byte> out=output;
        auto object=std::as_writable_bytes(std::span{&encoder,1});
        const std::array regions{std::span{s.raw},std::span{s.serialized},s.views(),object};
        if(which<4) in=regions[which].first(1);
        else if(which<8) out=regions[which-4].first(1);
        else out=std::span{input};
        const auto result=encoder.process(in,out,0);
        EXPECT_EQ(result.status,Status::error); EXPECT_EQ(result.error.code,Code::invalid_argument);
        EXPECT_EQ(result.input_consumed,0); EXPECT_EQ(result.output_produced,0);
    }
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FailedSecondFramePublishesOnlyEarlierFrame) {
    for(auto search:{Search::indexed,Search::indexed_five_prefix}) {
    const auto stream=stream_for(512,256); Storage s(stream,search);
    s.limits.max_expansion_ratio=1; s.limits.expansion_slack=0;
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views(),3,search);
    std::vector<std::byte> input(512,std::byte{'a'}),output(16384,std::byte{0xa5});
    for(std::size_t i=0;i<256;++i) input[i]=std::byte(i);
    const auto expected=oracle(input,stream,3,Search::indexed);
    const auto first_only=oracle(std::span{input}.first(256),stream_for(256,256),3,Search::indexed);
    const auto result=encoder.process(input,output,end_flag);
    ASSERT_EQ(result.status,Status::error);
    EXPECT_EQ(result.error.code,Code::limit_exceeded); EXPECT_EQ(result.error.byte_position,256);
    EXPECT_EQ(result.output_produced,first_only.size()); EXPECT_EQ(result.input_consumed,512);
    EXPECT_EQ(encoder.frame_preparation_count(),2);
    EXPECT_TRUE(std::equal(output.begin(),output.begin()+result.output_produced,expected.begin()));
    EXPECT_TRUE(std::ranges::all_of(std::span{output}.subspan(result.output_produced),[](auto b){return b==std::byte{0xa5};}));
    const auto again=encoder.process({},output,end_flag);
    EXPECT_EQ(again.status,Status::error); EXPECT_EQ(again.output_produced,0);
    EXPECT_EQ(again.error.code,result.error.code); EXPECT_EQ(again.error.byte_position,256);
    }
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, SharedHeaderWriterValidatesBeforePublication) {
    auto stream=stream_for(0); std::array<std::byte,112> header{};
    ASSERT_TRUE(serialize_lzss_position_distance_1m_stream_header(stream,{},header));
    EXPECT_EQ(std::vector<std::byte>(header.begin(),header.end()),oracle({},stream,3,Search::indexed));
    const auto saved=header; stream.context_variant=8;
    EXPECT_FALSE(serialize_lzss_position_distance_1m_stream_header(stream,{},header)); EXPECT_EQ(header,saved);
}
TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, OwnedBudgetsRoundTripAndObjectOverlap) {
    using Owner=LzssPositionDistance1mFinderScratchOwnedEncoder;
    for(auto search:{Search::indexed,Search::indexed_five_prefix})
    for(auto size:{0U,1U,63U,64U,65U,197U,1048577U}) {
        const auto stream=stream_for(size,size>1000000?1048576:64);
        LzssPositionDistanceWorkspaceRequirements required{};
        marc::core::DecoderLimits limits{};limits.max_block_size=stream.frame_size;
        ASSERT_EQ(Owner::requirements(stream,limits,required,search),Code::none);
        const auto saved=required;
        limits.max_internal_buffered_bytes=required.aggregate_bytes-1;
        EXPECT_EQ(Owner::requirements(stream,limits,required,search),Code::limit_exceeded);
        EXPECT_EQ(required,saved);
        Code error{};EXPECT_FALSE(Owner::create(stream,limits,error,3,search));
        EXPECT_EQ(error,Code::limit_exceeded);
        ++limits.max_internal_buffered_bytes;
        auto encoder=Owner::create(stream,limits,error,3,search);ASSERT_TRUE(encoder);ASSERT_EQ(error,Code::none);
        std::vector<std::byte> input(size);
        for(std::size_t i=0;i<input.size();++i) input[i]=std::byte(i%251);
        std::vector<std::byte> encoded;std::array<std::byte,997> buffer{};
        std::size_t pos=0;
        for(std::size_t calls=0;calls<2000000;++calls) {
            const auto count=std::min<std::size_t>(8191,input.size()-pos);
            const auto in=std::span{input}.subspan(pos,count);
            const auto cap=calls%7==0?0:buffer.size();
            const auto q=encoder->process(in,std::span{buffer}.first(cap),pos+count==input.size()?end_flag:flush_flag);
            ASSERT_TRUE(marc::core::is_valid(q,in.size(),cap));ASSERT_NE(q.status,Status::error);
            pos+=q.input_consumed;encoded.insert(encoded.end(),buffer.begin(),buffer.begin()+q.output_produced);
            if(q.status==Status::end_of_stream) break;
        }
        ASSERT_EQ(pos,input.size());
        EXPECT_EQ(encoder->process({}, {},end_flag).status,Status::end_of_stream);
        EXPECT_EQ(encoded,oracle(input,stream,3,Search::indexed_reference));
        auto decoder=LzssPositionDistance1mOwnedDecoder::create(stream.frame_size,{},error);ASSERT_TRUE(decoder);
        std::vector<std::byte> decoded;pos=0;
        for(std::size_t calls=0;calls<2000000;++calls) {
            const auto in=std::span{encoded}.subspan(pos,std::min<std::size_t>(103,encoded.size()-pos));
            const auto q=decoder->process(in,buffer,pos+in.size()==encoded.size()?end_flag:0);
            ASSERT_TRUE(marc::core::is_valid(q,in.size(),buffer.size()));ASSERT_NE(q.status,Status::error);
            pos+=q.input_consumed;decoded.insert(decoded.end(),buffer.begin(),buffer.begin()+q.output_produced);
            if(q.status==Status::end_of_stream) break;
        }
        EXPECT_EQ(decoded,input);EXPECT_EQ(pos,encoded.size());
    }
    Code error{};auto owner=Owner::create(stream_for(1),{},error);ASSERT_TRUE(owner);
    auto bytes=std::as_writable_bytes(std::span{owner.get(),1});
    EXPECT_EQ(owner->process({},bytes.first(1),0).error.code,Code::invalid_argument);
    EXPECT_EQ(owner->process({}, {},0).status,Status::error);
    auto invalid=stream_for(0);invalid.context_variant=9;
    EXPECT_FALSE(Owner::create(invalid,{},error));EXPECT_EQ(error,Code::invalid_argument);
    EXPECT_FALSE(Owner::create(stream_for(0),{},error,2));EXPECT_EQ(error,Code::invalid_argument);
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, WorkspaceLimitsAlignmentAndUnusedCapacity) {
    const auto stream=stream_for(21);Storage s(stream);
    auto r=s.requirements;
    EXPECT_EQ(r.operation_count,2*stream.frame_size);
    EXPECT_EQ(r.finder_bytes,8*(65536+stream.frame_size));
    EXPECT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,SIZE_MAX,r),LzssPositionDistanceWorkspaceError::arithmetic_overflow);
    EXPECT_EQ(r,s.requirements);
    std::vector<std::byte> oversized(s.raw.size()+1);
    Encoder extra(stream,s.limits,oversized,s.serialized,s.views());
    EXPECT_EQ(extra.process({}, {},0).error.code,Code::limit_exceeded);
    LzssPositionDistanceWorkspaceViews v{};
    std::vector<std::byte> unaligned(s.requirements.views_bytes+1);
    EXPECT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),s.raw,s.serialized,
        std::span{unaligned}.subspan(1),v),LzssPositionDistanceWorkspaceError::misaligned);
    EXPECT_TRUE(v.raw.empty());
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, LatchedEndDrainsWithoutRepeatingFlag) {
    const auto stream=stream_for(3);Storage s(stream);
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views());
    std::array<std::byte,3> input{};std::array<std::byte,112> header{};
    const auto first=encoder.process(input,header,end_flag);
    ASSERT_EQ(first.input_consumed,3);ASSERT_EQ(first.output_produced,112);
    ASSERT_EQ(first.status,Status::need_output);
    std::vector<std::byte> bytes(header.begin(),header.end());
    Status status{};
    for(unsigned call=0;call<1024;++call) {
        std::array<std::byte,1> output{};const auto q=encoder.process({},output,0);
        ASSERT_NE(q.status,Status::error);
        if(q.output_produced) bytes.push_back(output[0]);
        status=q.status;if(status==Status::end_of_stream) break;
    }
    EXPECT_EQ(status,Status::end_of_stream);EXPECT_EQ(encoder.frame_preparation_count(),1);
    EXPECT_EQ(bytes,oracle(input,stream,3,Search::exhaustive));
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FivePrefixWorkspaceAdmissionAndRawFailure) {
    using E=LzssPositionDistanceWorkspaceError;
    constexpr auto five=Search::indexed_five_prefix;
    for(auto frame:{1U,2U,3U,4U,5U,1048576U}) {
        const auto stream=stream_for(frame,frame);
        Storage old(stream),s(stream,five);
        const auto extra=frame<3?0U:4U*(65536+frame);
        EXPECT_EQ(s.requirements.finder_bytes,old.requirements.finder_bytes+extra);
        EXPECT_EQ(s.requirements.aggregate_bytes,old.requirements.aggregate_bytes+extra);
        auto r=s.requirements;const auto saved=r;
        auto limits=s.limits;--limits.max_internal_buffered_bytes;
        EXPECT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),r,five),E::limit_exceeded);
        EXPECT_EQ(r,saved);
        EXPECT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,s.limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),r,static_cast<Search>(99)),
            E::invalid_configuration);
        EXPECT_EQ(r,saved);
        LzssPositionDistanceWorkspaceViews views{};
        std::fill(s.views().begin(),s.views().end(),std::byte{0xa5});
        const auto before=std::vector<std::byte>(s.views().begin(),s.views().end());
        EXPECT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),
            s.raw,s.serialized,s.views(),views,five),E::limit_exceeded);
        EXPECT_TRUE(views.tokens.empty());
        EXPECT_TRUE(std::equal(before.begin(),before.end(),s.views().begin()));
        EXPECT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,s.limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),
            s.raw,s.serialized,s.views().first(s.views().size()-1),views,five),E::too_small);
        EXPECT_TRUE(views.tokens.empty());
        EXPECT_TRUE(std::equal(before.begin(),before.end(),s.views().begin()));
        ASSERT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,s.limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),
            s.raw,s.serialized,s.views(),views,five),E::none);
        s.serialized.assign(s.serialized.size(),std::byte{0xa5});
        const auto result=encode_lzss_position_distance_1m_finder_scratch_raw_frame(stream,s.limits,0,0,
            s.raw,3,five,views.tokens,views.operations,views.finder,std::span{s.serialized}.first(1));
        EXPECT_EQ(result.error,LzssPositionDistanceRawFrameError::frame_error);
        EXPECT_TRUE(std::ranges::all_of(s.serialized,[](auto b){return b==std::byte{0xa5};}));
    }
    const auto stream=stream_for(21);
    Storage old(stream);
    Encoder short_storage(stream,old.limits,old.raw,old.serialized,old.views(),3,five);
    std::array<std::byte,112> output{};output.fill(std::byte{0xa5});
    const auto r=short_storage.process({},output,0);
    EXPECT_EQ(r.status,Status::error);EXPECT_EQ(r.output_produced,0);
    EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xa5};}));
}

TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FivePrefixWideHistoryAndShortFinalFrames) {
    std::vector<std::byte> input(1048576+5);std::uint32_t seed=719;
    for(std::size_t i=0;i<70001;++i) {seed=seed*1664525U+1013904223U;input[i]=std::byte(seed>>24);}
    for(std::size_t i=70001;i<input.size();++i) input[i]=input[i%70001];
    for(unsigned policy:{3U,4U,5U}) for(unsigned tail:{1U,2U,3U,4U,5U}) {
        const auto raw=std::span{input}.first(1048576+tail);
        const auto stream=stream_for(raw.size(),1048576);
        EXPECT_EQ(run(raw,stream,policy,Search::indexed_five_prefix,8191,4093,true),
            oracle(raw,stream,policy,Search::indexed));
    }
}

void same_frame_result(const LzssShortMatchFrameEncodeResult& a,const LzssShortMatchFrameEncodeResult& b) {
#define CHECK(field) EXPECT_EQ(a.field,b.field)
    CHECK(error);CHECK(preflight_error);CHECK(serialized_size);CHECK(raw_size);CHECK(token_count);
    CHECK(operation_count);CHECK(decision_count);CHECK(payload_size);
    CHECK(context.error);CHECK(context.token_error);CHECK(context.token_count);CHECK(context.token_index);
    CHECK(context.operation_count);CHECK(context.operation_index);CHECK(context.decision_count);CHECK(context.raw_size);
    CHECK(entropy.error);CHECK(entropy.operation_count);CHECK(entropy.operation_index);CHECK(entropy.decision_count);CHECK(entropy.payload_size);
#undef CHECK
}
TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FrameBudgetCapacityAndMalformedErrorsMatchExactly) {
    using Token=marc::dictionary::internal::LzssTypedToken;
    using Kind=marc::dictionary::internal::LzssTypedTokenKind;
    using Operation=marc::context::internal::ModeledOperation;
    const auto stream=stream_for(4,4);
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
    std::array<Operation,10> op_a{},op_b{};std::array<std::byte,256> a{},b{},scratch{};
    std::vector<std::size_t> budgets;
    for(std::size_t n=1;n<=512;++n)budgets.push_back(n);
    const auto state=std::max(marc::entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes(),
        sizeof(marc::entropy::internal::LzssPositionDistance1mRangeDecoder));
    for(std::size_t n=state;n<state+512;++n)budgets.push_back(n);
    budgets.push_back(128U*1024U*1024U);
    for(auto budget:budgets)for(auto cap:{0U,1U,80U,256U})for(auto count:{0U,1U,10U}) {
        marc::core::DecoderLimits limits{};limits.max_block_size=4;limits.max_internal_buffered_bytes=budget;
        a.fill(std::byte{0xa5});b=a;
        const auto x=encode_lzss_position_distance_1m_frame(stream,limits,0,0,tokens,std::span{op_a}.first(count),std::span{a}.first(cap));
        const auto y=encode_lzss_position_distance_1m_finder_scratch_frame(stream,limits,0,0,tokens,std::span{op_b}.first(count),std::span{b}.first(cap),scratch);
        same_frame_result(x,y);EXPECT_EQ(a,b);
        if(y.error!=LzssShortMatchFrameEncodeError::none) EXPECT_TRUE(std::all_of(b.begin(),b.end(),[](auto v){return v==std::byte{0xa5};}));
    }
    for(unsigned mode=0;mode<6;++mode) {
        auto bad=tokens;auto config=stream;std::uint64_t seq=0,committed=0;
        if(mode==0)bad[1].distance=2;
        if(mode==1)bad[1].length=2;
        if(mode==2)bad[1].literal=9;
        if(mode==3)config.context_variant=99;
        if(mode==4)seq=1;
        if(mode==5)committed=1;
        a.fill(std::byte{0xa5});b=a;
        const auto x=encode_lzss_position_distance_1m_frame(config,{},seq,committed,bad,op_a,a);
        const auto y=encode_lzss_position_distance_1m_finder_scratch_frame(config,{},seq,committed,bad,op_b,b,scratch);
        ASSERT_NE(x.error,LzssShortMatchFrameEncodeError::none);same_frame_result(x,y);EXPECT_EQ(a,b);
        EXPECT_TRUE(std::all_of(b.begin(),b.end(),[](auto v){return v==std::byte{0xa5};}));
    }
}
TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, FrameOutputAliasRejectsWithoutWrites) {
    using Token=marc::dictionary::internal::LzssTypedToken;using Kind=marc::dictionary::internal::LzssTypedTokenKind;
    std::array<Token,2> tokens{{{Kind::literal,65,0,0},{Kind::match,0,1,3}}};
    const auto stream=stream_for(4,4);std::array<marc::context::internal::ModeledOperation,10> operations{};
    const auto token_bytes=std::as_writable_bytes(std::span{tokens});const std::vector<std::byte> saved(token_bytes.begin(),token_bytes.end());
    const auto a=encode_lzss_position_distance_1m_frame(stream,{},0,0,tokens,operations,token_bytes);
    std::array<std::byte,256> scratch{};
    const auto b=encode_lzss_position_distance_1m_finder_scratch_frame(stream,{},0,0,tokens,operations,token_bytes,scratch);
    EXPECT_EQ(a.error,LzssShortMatchFrameEncodeError::overlapping_workspaces);same_frame_result(a,b);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),token_bytes.begin()));
}
TEST(LzssPositionDistance1mFinderScratchStreamingEncoder, OwnerRequirementsAndExactBudgetsUnchanged) {
    using Base=LzssPositionDistance1mPreparedOwnedEncoder;using Trial=LzssPositionDistance1mFinderScratchOwnedEncoder;
    static_assert(sizeof(Base)==sizeof(Trial) && alignof(Base)==alignof(Trial));
    static_assert(sizeof(Encoder)==sizeof(LzssPositionDistance1mPreparedFrameStreamingEncoder));
    for(unsigned frame:{1U,2U,3U,65536U,1048576U}) {
        const auto stream=stream_for(frame,frame);marc::core::DecoderLimits limits{};limits.max_block_size=frame;
        LzssPositionDistanceWorkspaceRequirements a{},b{};
        ASSERT_EQ(Base::requirements(stream,limits,a,Search::indexed_five_prefix),Code::none);
        ASSERT_EQ(Trial::requirements(stream,limits,b,Search::indexed_five_prefix),Code::none);EXPECT_EQ(a,b);
        limits.max_internal_buffered_bytes=a.aggregate_bytes;Code error{};
        EXPECT_TRUE(Trial::create(stream,limits,error,3,Search::indexed_five_prefix));EXPECT_EQ(error,Code::none);
        --limits.max_internal_buffered_bytes;
        EXPECT_FALSE(Trial::create(stream,limits,error,3,Search::indexed_five_prefix));EXPECT_EQ(error,Code::limit_exceeded);
    }
}
} // namespace

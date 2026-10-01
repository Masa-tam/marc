#include "frame/lzss_position_distance_4m_end_probe_scalar_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_end_probe_scalar_owned_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_owned_decoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <iostream>

namespace {
using namespace marc::frame::internal;
using Encoder=LzssPositionDistance4mEndProbeScalarFrameStreamingEncoder;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
using Search=marc::dictionary::internal::LzssPositionDistance4mSearch;
constexpr auto end_flag=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush_flag=marc::core::flag_value(marc::core::ProcessFlags::flush);
TypedContextStreamHeader stream_for(std::size_t size,std::uint32_t frame=21) {
    return {frame,size,{4194304,3,258,0},32768,46,10,1,11};
}
struct Storage {
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements requirements{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> aligned;
    explicit Storage(TypedContextStreamHeader stream) {
        limits.max_block_size=stream.frame_size;limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U*1024U*1024U;
        EXPECT_EQ(calculate_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),requirements),
            LzssPositionDistanceWorkspaceError::none);
        raw.resize(requirements.raw_bytes); serialized.resize(requirements.serialized_bytes);
        aligned.resize((requirements.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        limits.max_internal_buffered_bytes=requirements.aggregate_bytes;
    }
    std::span<std::byte> views() { return std::as_writable_bytes(std::span{aligned}).first(requirements.views_bytes); }
};
struct ReferenceStorage {
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements requirements{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> aligned;
    explicit ReferenceStorage(TypedContextStreamHeader stream,Search search=Search::indexed_reference) {
        limits.max_block_size=stream.frame_size;limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U*1024U*1024U;
        EXPECT_EQ(calculate_lzss_position_distance_4m_encode_workspace(stream,limits,
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
    ReferenceStorage s(stream,search); LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_4m_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),s.raw,s.serialized,s.views(),v,search),
        LzssPositionDistanceWorkspaceError::none);
    const auto frames=input.size()/stream.frame_size+(input.size()%stream.frame_size!=0);
    std::vector<std::byte> result(112+frames*s.requirements.serialized_bytes);
    std::array<std::byte,112> header{};
    EXPECT_TRUE(serialize_lzss_position_distance_4m_stream_header(stream,s.limits,header));
    std::copy(header.begin(),header.end(),result.begin());
    std::size_t written=112,position=0;std::uint64_t sequence=0;
    while(position<input.size()) {
        const auto count=std::min<std::size_t>(stream.frame_size,input.size()-position);
        const auto frame=encode_lzss_position_distance_4m_raw_frame(stream,s.limits,sequence++,position,
            input.subspan(position,count),eligibility,search,v.tokens,v.operations,v.finder,v.serialized);
        EXPECT_EQ(frame.error,LzssPositionDistanceRawFrameError::none);
        std::copy_n(v.serialized.begin(),frame.frame.serialized_size,result.begin()+written);
        written+=frame.frame.serialized_size;position+=count;
    }
    result.resize(written); return result;
}
std::vector<std::byte> run(std::span<const std::byte> input,TypedContextStreamHeader stream,
    unsigned eligibility,std::size_t in_chunk,std::size_t out_chunk,bool random=false) {
    Storage s(stream);
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views(),eligibility);
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

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, MatchesOracleAcrossPoliciesAndFrameBoundaries) {
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
        EXPECT_EQ(oracle(input,stream,policy,Search::indexed_reference),reference);
        EXPECT_EQ(run(input,stream,policy,1,1),reference);
        EXPECT_EQ(run(input,stream,policy,13,7),reference);
        EXPECT_EQ(run(input,stream,policy,79,31,true),reference);
    }
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, EverySmallInputChunkAndMaximumFrame) {
    std::vector<std::byte> input(49,std::byte{'a'});
    const auto stream=stream_for(input.size()); const auto expected=oracle(input,stream,3,Search::indexed_reference);
    for(std::size_t chunk=1;chunk<=input.size();++chunk)
        EXPECT_EQ(run(input,stream,3,chunk,7),expected);
    input.resize(4194305);
    for(std::size_t i=0;i<input.size();++i) input[i]=std::byte(i%251);
    EXPECT_EQ(run(input,stream_for(input.size(),4194304),3,8191,4093,true),
        oracle(input,stream_for(input.size(),4194304),3,Search::indexed_reference));
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, FlushStarvationAndDelayedEndDoNotPrepareTwice) {
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
    EXPECT_EQ(encoded,oracle(input,stream,3,Search::indexed_reference));
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, RejectsWrongSizesFlagsAndKeepsErrorsSticky) {
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

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, ConstructorRejectsShortStorageLimitsAndInvalidPolicy) {
    for(unsigned which=0;which<6;++which) {
        Storage s(stream_for(21)); auto raw=std::span{s.raw},serialized=std::span{s.serialized},aligned=s.views();
        unsigned policy=3;
        if(which==0) raw=raw.first(raw.size()-1);
        if(which==1) serialized=serialized.first(serialized.size()-1);
        if(which==2) aligned=aligned.first(aligned.size()-1);
        if(which==3) --s.limits.max_internal_buffered_bytes;
        if(which==4) policy=2;
        if(which==5) raw=serialized.first(21);
        Encoder encoder(stream_for(21),s.limits,raw,serialized,aligned,policy);
        std::array<std::byte,112> output{}; output.fill(std::byte{0xa5});
        const auto result=encoder.process({},output,0);
        EXPECT_EQ(result.status,Status::error); EXPECT_EQ(result.output_produced,0);
        EXPECT_EQ(result.error.code,which<3?Code::out_of_memory:which==3?Code::limit_exceeded:Code::invalid_argument);
        EXPECT_EQ(encoder.frame_preparation_count(),0);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto x){return x==std::byte{0xa5};}));
    }
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, RejectsProcessOverlapWithEveryLiveRegion) {
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

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, FailedSecondFramePublishesOnlyEarlierFrame) {
    {
    const auto stream=stream_for(512,256); Storage s(stream);
    s.limits.max_expansion_ratio=1; s.limits.expansion_slack=0;
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views(),3);
    std::vector<std::byte> input(512,std::byte{'a'}),output(16384,std::byte{0xa5});
    for(std::size_t i=0;i<256;++i) input[i]=std::byte(i);
    const auto expected=oracle(input,stream,3,Search::indexed_reference);
    const auto first_only=oracle(std::span{input}.first(256),stream_for(256,256),3,Search::indexed_reference);
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

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, SharedHeaderWriterValidatesBeforePublication) {
    auto stream=stream_for(0); std::array<std::byte,112> header{};
    ASSERT_TRUE(serialize_lzss_position_distance_4m_stream_header(stream,{},header));
    EXPECT_EQ(std::vector<std::byte>(header.begin(),header.end()),oracle({},stream,3,Search::indexed_reference));
    const auto saved=header; stream.context_variant=8;
    EXPECT_FALSE(serialize_lzss_position_distance_4m_stream_header(stream,{},header)); EXPECT_EQ(header,saved);
}
TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, OwnedBudgetsRoundTripAndObjectOverlap) {
    using Owner=LzssPositionDistance4mEndProbeScalarOwnedEncoder;
    for(auto size:{0U,1U,63U,64U,65U,197U,4194305U}) {
        const auto stream=stream_for(size,size>1000000?4194304:64);
        LzssPositionDistanceWorkspaceRequirements required{};
        marc::core::DecoderLimits limits{};limits.max_block_size=stream.frame_size;limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U*1024U*1024U;
        ASSERT_EQ(Owner::requirements(stream,limits,required),Code::none);
        const auto saved=required;
        limits.max_internal_buffered_bytes=required.aggregate_bytes-1;
        EXPECT_EQ(Owner::requirements(stream,limits,required),Code::limit_exceeded);
        EXPECT_EQ(required,saved);
        Code error{};EXPECT_FALSE(Owner::create(stream,limits,error,3));
        EXPECT_EQ(error,Code::limit_exceeded);
        ++limits.max_internal_buffered_bytes;
        auto encoder=Owner::create(stream,limits,error,3);ASSERT_TRUE(encoder);ASSERT_EQ(error,Code::none);
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
        auto decoder=LzssPositionDistance4mOwnedDecoder::create(stream.frame_size,limits,error);ASSERT_TRUE(decoder);
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

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, WorkspaceLimitsAlignmentAndUnusedCapacity) {
    const auto stream=stream_for(21);Storage s(stream);
    auto r=s.requirements;
    EXPECT_EQ(r.operation_count,2*stream.frame_size);
    EXPECT_EQ(r.finder_bytes,12*(65536+stream.frame_size));
    EXPECT_EQ(calculate_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,SIZE_MAX,r),LzssPositionDistanceWorkspaceError::arithmetic_overflow);
    EXPECT_EQ(r,s.requirements);
    std::vector<std::byte> oversized(s.raw.size()+1);
    Encoder extra(stream,s.limits,oversized,s.serialized,s.views());
    EXPECT_EQ(extra.process({}, {},0).error.code,Code::limit_exceeded);
    LzssPositionDistanceWorkspaceViews v{};
    std::vector<std::byte> unaligned(s.requirements.views_bytes+1);
    EXPECT_EQ(partition_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),s.raw,s.serialized,
        std::span{unaligned}.subspan(1),v),LzssPositionDistanceWorkspaceError::misaligned);
    EXPECT_TRUE(v.raw.empty());
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, WorkspaceFailuresPreserveMetadataAndEveryCapacityIsCharged) {
    using E=LzssPositionDistanceWorkspaceError;
    const auto stream=stream_for(21);Storage s(stream);
    auto r=s.requirements;const auto saved=r;
    EXPECT_EQ(calculate_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::decode,0,r),E::invalid_configuration);
    EXPECT_EQ(r,saved);
    auto invalid=stream;invalid.context_variant=9;
    EXPECT_EQ(calculate_lzss_position_distance_4m_end_probe_scalar_encode_workspace(invalid,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,0,r),E::invalid_configuration);
    EXPECT_EQ(r,saved);
    std::size_t charged=17;
    EXPECT_EQ(charge_lzss_position_distance_4m_end_probe_scalar_encode_workspace(s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,SIZE_MAX,1,1,1,charged),E::arithmetic_overflow);
    EXPECT_EQ(charged,17);
    for(unsigned which=0;which<3;++which) {
        std::vector<std::byte> raw(s.raw.size()+(which==0)),serialized(s.serialized.size()+(which==1));
        std::vector<std::max_align_t> backing((r.views_bytes+1+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        auto storage=std::as_writable_bytes(std::span{backing}).first(r.views_bytes+(which==2));
        LzssPositionDistanceWorkspaceViews views{};
        EXPECT_EQ(partition_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,s.limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),raw,serialized,storage,views),E::limit_exceeded);
        EXPECT_TRUE(views.raw.empty());EXPECT_TRUE(views.tokens.empty());
    }
    LzssPositionDistanceWorkspaceViews views{};
    const auto alias=std::as_writable_bytes(std::span{&views,1}).first(21);
    EXPECT_EQ(partition_lzss_position_distance_4m_end_probe_scalar_encode_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),alias,s.serialized,s.views(),views),E::overlapping_buffers);
    EXPECT_TRUE(views.raw.empty());
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, FullLayoutRetainsReferenceAndAddsOnlyFinderStorage) {
    const auto stream=stream_for(0,4194304);Storage s(stream);ReferenceStorage reference(stream);
    EXPECT_EQ(s.requirements.finder_bytes,51118080);
    EXPECT_EQ(reference.requirements.finder_bytes,17039360);
    EXPECT_EQ(s.requirements.views_bytes-reference.requirements.views_bytes,34078720);
    EXPECT_EQ(s.requirements.raw_bytes,reference.requirements.raw_bytes);
    EXPECT_EQ(s.requirements.serialized_bytes,reference.requirements.serialized_bytes);
    EXPECT_EQ(s.requirements.operation_count,reference.requirements.operation_count);
    EXPECT_EQ(s.requirements.model_state_bytes,reference.requirements.model_state_bytes);
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, CorpusOwnerMatchesFrozenStreamAndRestores) {
    const auto* input_path=std::getenv("MARC_POSITION_4M_FIVE_OWNER_INPUT");
    const auto* archive_path=std::getenv("MARC_POSITION_4M_FIVE_OWNER_ARCHIVE");
    if(!input_path||!archive_path)GTEST_SKIP()<<"Optional corpus owner check";
    const auto read=[](const char* path,std::size_t maximum) {
        std::ifstream f(path,std::ios::binary|std::ios::ate);
        if(!f)return std::vector<std::byte>{};
        const auto size=f.tellg();if(size<0||static_cast<std::uint64_t>(size)>maximum)return std::vector<std::byte>{};
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));f.seekg(0);
        if(size!=0&&!f.read(reinterpret_cast<char*>(bytes.data()),size))return std::vector<std::byte>{};
        return bytes;
    };
    const auto input=read(input_path,64U*1024U*1024U),archive=read(archive_path,128U*1024U*1024U);
    ASSERT_FALSE(input.empty());ASSERT_GT(archive.size(),112);
    const auto stream=stream_for(input.size(),4194304);
    marc::core::DecoderLimits limits{};limits.max_block_size=4194304;
    limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U*1024U*1024U;
    using Owner=LzssPositionDistance4mEndProbeScalarOwnedEncoder;
    LzssPositionDistanceWorkspaceRequirements required{};
    ASSERT_EQ(Owner::requirements(stream,limits,required),Code::none);
    Code error{};auto encoder=Owner::create(stream,limits,error);ASSERT_TRUE(encoder);
    auto decoder=LzssPositionDistance4mOwnedDecoder::create(4194304,limits,error);ASSERT_TRUE(decoder);
    const auto verify=[](marc::core::Transform& transform,std::span<const std::byte> source,
        std::span<const std::byte> expected) {
        std::array<std::byte,65536> buffer{};std::size_t consumed=0,produced=0;
        for(std::size_t calls=0;calls<20000000;++calls) {
            const auto in=source.subspan(consumed,std::min(buffer.size(),source.size()-consumed));
            const auto q=transform.process(in,buffer,consumed+in.size()==source.size()?end_flag:0);
            if(!marc::core::is_valid(q,in.size(),buffer.size())||q.status==Status::error
                ||produced>expected.size()||q.output_produced>expected.size()-produced)return false;
            if(!std::equal(buffer.begin(),buffer.begin()+q.output_produced,expected.begin()+produced))return false;
            consumed+=q.input_consumed;produced+=q.output_produced;
            if(q.status==Status::end_of_stream)return consumed==source.size()&&produced==expected.size();
        }
        return false;
    };
    ASSERT_TRUE(verify(*encoder,input,archive));ASSERT_TRUE(verify(*decoder,archive,input));
    std::cout<<"CORPUS frames="<<(input.size()+4194303)/4194304<<" raw="<<input.size()
        <<" archive="<<archive.size()<<" budget="<<required.aggregate_bytes<<" finder="<<required.finder_bytes<<'\n';
}

TEST(LzssPositionDistance4mEndProbeScalarStreamingEncoder, LatchedEndDrainsWithoutRepeatingFlag) {
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

}

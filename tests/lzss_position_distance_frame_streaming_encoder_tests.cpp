#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "frame/lzss_position_distance_stream_encoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Encoder=LzssPositionDistanceFrameStreamingEncoder;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
using Search=LzssPositionDistanceSearch;
constexpr auto end_flag=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush_flag=marc::core::flag_value(marc::core::ProcessFlags::flush);
TypedContextStreamHeader stream_for(std::size_t size,std::uint32_t frame=21) {
    return {frame,size,{65536,3,258,0},32768,40,8,1,9};
}
struct Storage {
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements requirements{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> aligned;
    explicit Storage(TypedContextStreamHeader stream) {
        limits.max_block_size=stream.frame_size;
        EXPECT_EQ(calculate_lzss_position_distance_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),requirements),
            LzssPositionDistanceWorkspaceError::none);
        raw.resize(requirements.raw_bytes); serialized.resize(requirements.serialized_bytes);
        aligned.resize((requirements.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        limits.max_internal_buffered_bytes=requirements.aggregate_bytes;
    }
    std::span<std::byte> views() { return std::as_writable_bytes(std::span{aligned}).first(requirements.views_bytes); }
};
std::vector<std::byte> oracle(std::span<const std::byte> input,TypedContextStreamHeader stream,
    unsigned eligibility,Search search) {
    Storage s(stream); LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_workspace(stream,s.limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(Encoder),s.raw,s.serialized,s.views(),v),
        LzssPositionDistanceWorkspaceError::none);
    const auto frames=input.size()/stream.frame_size+(input.size()%stream.frame_size!=0);
    std::vector<std::byte> result(112+frames*s.requirements.serialized_bytes);
    const auto written=encode_lzss_position_distance_raw_stream(stream,s.limits,input,eligibility,
        search,v.tokens,v.operations,v.finder,result);
    EXPECT_EQ(written.error,LzssPositionDistanceRawStreamError::none);
    result.resize(written.serialized_size); return result;
}
std::vector<std::byte> run(std::span<const std::byte> input,TypedContextStreamHeader stream,
    unsigned eligibility,Search search,std::size_t in_chunk,std::size_t out_chunk,bool random=false) {
    Storage s(stream);
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views(),eligibility,search);
    std::vector<std::byte> encoded,buffer(out_chunk);
    std::size_t position{}; std::uint32_t seed=17;
    const auto frames=input.size()/stream.frame_size+(input.size()%stream.frame_size!=0);
    for (std::size_t calls=0;calls<1000000;++calls) {
        seed=seed*1664525U+1013904223U;
        auto extent=std::min(input.size()-position,random ? 1+seed%in_chunk : in_chunk);
        auto capacity=random ? 1+(seed>>16)%out_chunk : out_chunk;
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

TEST(LzssPositionDistanceStreamingEncoder, MatchesOracleAcrossPoliciesAndFrameBoundaries) {
    for (std::uint32_t frame:{1U,3U,21U,257U})
    for (std::size_t size:{std::size_t{0},std::size_t{1},std::size_t{frame-1},std::size_t{frame},std::size_t{frame+1},std::size_t{2*frame+7}})
    for(unsigned policy=3;policy<=5;++policy) {
        SCOPED_TRACE(frame);
        SCOPED_TRACE(size);
        SCOPED_TRACE(policy);
        std::vector<std::byte> input(size);
        for(std::size_t i=0;i<size;++i) input[i]=std::byte((i/17)%2 ? i%7 : (i*71+i/11)%256);
        const auto stream=stream_for(size,frame);
        const auto reference=oracle(input,stream,policy,Search::reference);
        EXPECT_EQ(oracle(input,stream,policy,Search::indexed),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed,1,1),reference);
        EXPECT_EQ(run(input,stream,policy,Search::reference,13,7),reference);
        EXPECT_EQ(run(input,stream,policy,Search::indexed,79,31,true),reference);
    }
}

TEST(LzssPositionDistanceStreamingEncoder, EverySmallInputChunkAndMaximumFrame) {
    std::vector<std::byte> input(49,std::byte{'a'});
    const auto stream=stream_for(input.size()); const auto expected=oracle(input,stream,3,Search::indexed);
    for(std::size_t chunk=1;chunk<=input.size();++chunk)
        EXPECT_EQ(run(input,stream,3,Search::indexed,chunk,7),expected);
    input.resize(65537);
    for(std::size_t i=0;i<input.size();++i) input[i]=std::byte(i%251);
    EXPECT_EQ(run(input,stream_for(input.size(),65536),3,Search::indexed,8191,4093,true),
        oracle(input,stream_for(input.size(),65536),3,Search::indexed));
}

TEST(LzssPositionDistanceStreamingEncoder, FlushStarvationAndDelayedEndDoNotPrepareTwice) {
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

TEST(LzssPositionDistanceStreamingEncoder, RejectsWrongSizesFlagsAndKeepsErrorsSticky) {
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

TEST(LzssPositionDistanceStreamingEncoder, ConstructorRejectsShortStorageLimitsAndInvalidPolicy) {
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

TEST(LzssPositionDistanceStreamingEncoder, RejectsProcessOverlapWithEveryLiveRegion) {
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

TEST(LzssPositionDistanceStreamingEncoder, FailedSecondFramePublishesOnlyEarlierFrame) {
    const auto stream=stream_for(512,256); Storage s(stream);
    s.limits.max_expansion_ratio=1; s.limits.expansion_slack=0;
    Encoder encoder(stream,s.limits,s.raw,s.serialized,s.views());
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
}

TEST(LzssPositionDistanceStreamingEncoder, SharedHeaderWriterValidatesBeforePublication) {
    auto stream=stream_for(0); std::array<std::byte,112> header{};
    ASSERT_TRUE(serialize_lzss_position_distance_stream_header(stream,{},header));
    EXPECT_EQ(std::vector<std::byte>(header.begin(),header.end()),oracle({},stream,3,Search::indexed));
    const auto saved=header; stream.context_variant=8;
    EXPECT_FALSE(serialize_lzss_position_distance_stream_header(stream,{},header)); EXPECT_EQ(header,saved);
}
} // namespace

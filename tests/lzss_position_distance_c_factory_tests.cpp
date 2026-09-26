#include "marc/marc.h"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {
using Config = marc_lzss_position_distance_dynamic_range_config;
Config config_for(marc_direction direction, std::size_t size = 0) {
    Config c{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_config_init(direction, &c), MARC_STATUS_OK);
    c.original_size = size; c.frame_size = 21;
    c.max_frame_size = 21; c.max_block_size = 21;
    c.max_compressed_payload_size = 18 * 21 + 5;
    return c;
}
struct Storage {
    marc_workspace_requirements r{};
    std::vector<uint8_t> primary, secondary;
    std::vector<std::max_align_t> aligned;
    explicit Storage(const Config& c) {
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c, &r), MARC_STATUS_OK);
        primary.resize(r.primary_bytes + 64, 0xcd);
        secondary.resize(r.secondary_bytes + 64, 0xcd);
        aligned.resize((r.views_bytes + 64 + sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        std::memset(aligned.data(), 0xcd, aligned.size()*sizeof(std::max_align_t));
    }
    marc_buffer p() { return {primary.data(), primary.size()}; }
    marc_buffer s() { return {secondary.data(), secondary.size()}; }
    marc_buffer v() { return {reinterpret_cast<uint8_t*>(aligned.data()), aligned.size()*sizeof(std::max_align_t)}; }
    void tails() {
        EXPECT_TRUE(std::all_of(primary.begin()+r.primary_bytes, primary.end(), [](auto b){return b==0xcd;}));
        EXPECT_TRUE(std::all_of(secondary.begin()+r.secondary_bytes, secondary.end(), [](auto b){return b==0xcd;}));
        auto b=v();
        EXPECT_TRUE(std::all_of(b.data+r.views_bytes,b.data+b.size,[](auto x){return x==0xcd;}));
    }
};
std::vector<uint8_t> run(const Config& c, const std::vector<uint8_t>& input,
                        std::size_t chunk, std::size_t capacity) {
    Storage s(c); marc_transform* t{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),&t), MARC_STATUS_OK);
    if (!t) return {};
    std::vector<uint8_t> output, buffer(capacity);
    std::size_t offset{};
    bool ended=false;
    for (std::size_t calls=0; calls<100000; ++calls) {
        auto n=std::min(chunk,input.size()-offset);
        const auto flags=(offset+n==input.size()?MARC_PROCESS_END_INPUT:0u)
            | (calls%2?MARC_PROCESS_FLUSH:0u);
        auto r=marc_transform_process(t,{n?input.data()+offset:nullptr,n},{buffer.data(),buffer.size()},flags);
        EXPECT_LE(r.input_consumed,n); EXPECT_LE(r.output_produced,capacity);
        if (r.input_consumed>n || r.output_produced>capacity || r.status>=100) {
            ADD_FAILURE()<<r.status; break;
        }
        EXPECT_FALSE(r.status==MARC_STATUS_PROGRESS && !r.input_consumed && !r.output_produced);
        offset+=r.input_consumed;
        output.insert(output.end(),buffer.begin(),buffer.begin()+r.output_produced);
        if (r.status==MARC_STATUS_END_OF_STREAM) { ended=true; break; }
    }
    EXPECT_TRUE(ended); EXPECT_EQ(offset,input.size());
    EXPECT_EQ(marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_END_INPUT).status,MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(t); s.tails(); return output;
}
std::vector<uint8_t> oracle(const std::vector<uint8_t>& input) {
    using namespace marc::frame::internal;
    const TypedContextStreamHeader stream{21,input.size(),{65536,3,258,0},32768,40,8,1,9};
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements r{};
    EXPECT_EQ(calculate_lzss_position_distance_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistanceFrameStreamingEncoder),r),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> raw(r.raw_bytes), serialized(r.serialized_bytes);
    std::vector<std::max_align_t> storage((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistanceFrameStreamingEncoder),
        raw,serialized,std::as_writable_bytes(std::span{storage}).first(r.views_bytes),v),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<uint8_t> output(112+((input.size()+20)/21)*r.serialized_bytes);
    auto result=encode_lzss_position_distance_raw_stream(stream,limits,
        std::as_bytes(std::span{input}),3,LzssPositionDistanceSearch::reference,v.tokens,v.operations,v.finder,
        std::as_writable_bytes(std::span{output}));
    EXPECT_EQ(result.error,LzssPositionDistanceRawStreamError::none);
    output.resize(result.serialized_size); return output;
}
TEST(PositionDistanceCFactory, ChunkedBytesMatchPrivateOracleAndRoundTrip) {
    for (std::size_t size : {0u,1u,20u,21u,22u,49u,256u}) {
        SCOPED_TRACE(size);
        std::vector<uint8_t> input(size);
        for (std::size_t i=0;i<size;++i) input[i]=static_cast<uint8_t>((i/17)%2?i%7:i);
        const auto expected=oracle(input);
        for (std::size_t chunk : {1u,7u,512u}) {
            const auto encoded=run(config_for(MARC_DIRECTION_ENCODE,size),input,chunk,chunk);
            EXPECT_EQ(encoded,expected);
            EXPECT_EQ(run(config_for(MARC_DIRECTION_DECODE),encoded,chunk,chunk),input);
        }
    }
}
TEST(PositionDistanceCFactory, WorkspaceFailuresPublishNull) {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config_for(direction); Storage s(c);
        for (unsigned which=0;which<7;++which) {
            auto p=s.p(), q=s.s(), v=s.v();
            switch(which) {
            case 0:p.size=s.r.primary_bytes-1;break;
            case 1:q.size=s.r.secondary_bytes-1;break;
            case 2:v.size=s.r.views_bytes-1;break;
            case 3:++v.data;--v.size;break;
            case 4:q.data=p.data;break;
            case 5:v.data=p.data;break;
            case 6:p.data=nullptr;break;
            }
            auto* t=reinterpret_cast<marc_transform*>(std::uintptr_t{1});
            EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,p,q,v,&t),MARC_STATUS_INVALID_ARGUMENT);
            EXPECT_EQ(t,nullptr);
        }
        marc_transform* t{};
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(nullptr,s.p(),s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    }
}
TEST(PositionDistanceCFactory, MetadataAliasesAreRejectedWithoutWrites) {
    auto c=config_for(MARC_DIRECTION_ENCODE); Storage s(c);
    const auto before=c;
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),
        reinterpret_cast<marc_transform**>(&c)),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
    for (auto b : {s.p(),s.s(),s.v()}) {
        std::array<uint8_t,sizeof(marc_transform*)> saved{};
        std::memcpy(saved.data(),b.data,saved.size());
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),
            reinterpret_cast<marc_transform**>(b.data)),MARC_STATUS_INVALID_ARGUMENT);
        EXPECT_EQ(std::memcmp(saved.data(),b.data,saved.size()),0);
    }
    marc_transform* t{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,
        {reinterpret_cast<uint8_t*>(&c),s.r.primary_bytes},s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(t,nullptr); EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
}
TEST(PositionDistanceCFactory, ExactAggregateIncludesHandleAndUnusedTailsAreFree) {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config_for(direction); Storage s(c);
        uint64_t low=c.max_block_size,high=c.max_internal_buffered_bytes;
        marc_workspace_requirements r{};
        while(low<high) {
            c.max_internal_buffered_bytes=low+(high-low)/2;
            if(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c,&r)==MARC_STATUS_OK)
                high=c.max_internal_buffered_bytes;
            else low=c.max_internal_buffered_bytes+1;
        }
        c.max_internal_buffered_bytes=low;
        marc_transform* t{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
        marc_transform_destroy(t); s.tails();
        const auto empty_archive=oracle({});
        const auto output=run(c,direction==MARC_DIRECTION_ENCODE?std::vector<uint8_t>{}:empty_archive,1,1);
        EXPECT_EQ(output,direction==MARC_DIRECTION_ENCODE?empty_archive:std::vector<uint8_t>{});
        --c.max_internal_buffered_bytes;
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_EQ(t,nullptr);
    }
}
TEST(PositionDistanceCFactory, SecondFrameFailurePreservesOnlyFirstFrame) {
    auto bytes=oracle(std::vector<uint8_t>(42,42));
    const auto second=oracle(std::vector<uint8_t>(21,42)).size();
    ASSERT_LT(second,bytes.size()); bytes[second]^=1;
    for (std::size_t step : {1u,512u}) {
        auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
        std::vector<uint8_t> output; std::array<uint8_t,7> buffer{};
        std::size_t offset{}; marc_process_result result{};
        for (unsigned calls=0;calls<10000;++calls) {
            auto n=std::min(step,bytes.size()-offset);
            result=marc_transform_process(t,{bytes.data()+offset,n},{buffer.data(),buffer.size()},
                offset+n==bytes.size()?MARC_PROCESS_END_INPUT:MARC_PROCESS_NONE);
            ASSERT_LE(result.input_consumed,n); ASSERT_LE(result.output_produced,buffer.size());
            offset+=result.input_consumed;
            output.insert(output.end(),buffer.begin(),buffer.begin()+result.output_produced);
            if(result.status>=100 || result.status==MARC_STATUS_END_OF_STREAM) break;
        }
        EXPECT_EQ(result.status,MARC_STATUS_MALFORMED_STREAM);
        EXPECT_EQ(result.error_byte_position,second);
        EXPECT_EQ(output,std::vector<uint8_t>(21,42));
        EXPECT_EQ(marc_transform_process(t,{nullptr,0},{nullptr,0},0).status,result.status);
        marc_transform_destroy(t); s.tails();
    }
}
TEST(PositionDistanceCFactory, MalformedInputAndUnsupportedResetAreSticky) {
    auto encoded=oracle(std::vector<uint8_t>(49,42));
    encoded[0]^=1;
    auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
    ASSERT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
    std::array<uint8_t,64> output{};
    auto result=marc_transform_process(t,{encoded.data(),encoded.size()},{output.data(),output.size()},MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status,MARC_STATUS_MALFORMED_STREAM); EXPECT_EQ(result.output_produced,0u);
    auto sticky=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE);
    EXPECT_EQ(sticky.status,result.status); EXPECT_EQ(sticky.error_byte_position,result.error_byte_position);
    marc_transform_destroy(t);
    c=config_for(MARC_DIRECTION_ENCODE); Storage e(c);
    ASSERT_EQ(marc_lzss_position_distance_dynamic_range_create(&c,e.p(),e.s(),e.v(),&t),MARC_STATUS_OK);
    result=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_RESET_BLOCK);
    EXPECT_EQ(result.status,MARC_STATUS_UNSUPPORTED);
    EXPECT_EQ(marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE).status,result.status);
    marc_transform_destroy(t);
}
} // namespace

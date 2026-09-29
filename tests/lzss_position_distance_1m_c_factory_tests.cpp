#include "marc/marc.h"
#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_1m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include "frame/typed_context_format.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace {
using Config = marc_lzss_position_distance_dynamic_range_1m_config;
Config config_for(marc_direction direction, std::size_t size = 0) {
    Config c{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_config_init(direction, &c), MARC_STATUS_OK);
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
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(&c, &r), MARC_STATUS_OK);
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
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),&t), MARC_STATUS_OK);
    if (!t) return {};
    std::vector<uint8_t> output, buffer(capacity + 2, 0xcd);
    std::size_t offset{};
    bool ended=false;
    for (std::size_t calls=0; calls<100000; ++calls) {
        auto n=std::min(chunk,input.size()-offset);
        const auto flags=(offset+n==input.size()?MARC_PROCESS_END_INPUT:0u)
            | (calls%2?MARC_PROCESS_FLUSH:0u);
        auto r=marc_transform_process(t,{n?input.data()+offset:nullptr,n},{buffer.data()+1,capacity},flags);
        EXPECT_EQ(buffer.front(),0xcd); EXPECT_EQ(buffer.back(),0xcd);
        EXPECT_LE(r.input_consumed,n); EXPECT_LE(r.output_produced,capacity);
        if (r.input_consumed>n || r.output_produced>capacity || r.status>=100) {
            ADD_FAILURE()<<r.status; break;
        }
        EXPECT_FALSE(r.status==MARC_STATUS_PROGRESS && !r.input_consumed && !r.output_produced);
        offset+=r.input_consumed;
        output.insert(output.end(),buffer.begin()+1,buffer.begin()+1+r.output_produced);
        if (r.status==MARC_STATUS_END_OF_STREAM) { ended=true; break; }
    }
    EXPECT_TRUE(ended); EXPECT_EQ(offset,input.size());
    for (auto flags : {MARC_PROCESS_NONE, MARC_PROCESS_END_INPUT, MARC_PROCESS_FLUSH}) {
        const auto terminal=marc_transform_process(t,{nullptr,0},{nullptr,0},flags);
        EXPECT_EQ(terminal.status,MARC_STATUS_END_OF_STREAM);
        EXPECT_EQ(terminal.input_consumed,0u); EXPECT_EQ(terminal.output_produced,0u);
    }
    marc_transform_destroy(t); s.tails(); return output;
}
std::vector<uint8_t> oracle(const std::vector<uint8_t>& input, uint32_t frame=21) {
    using namespace marc::frame::internal;
    const TypedContextStreamHeader stream{frame,input.size(),{1048576,3,258,0},32768,44,9,1,10};
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements r{};
    EXPECT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance1mFrameStreamingEncoder),r),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> raw(r.raw_bytes), serialized(r.serialized_bytes);
    std::vector<std::max_align_t> storage((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_1m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance1mFrameStreamingEncoder),
        raw,serialized,std::as_writable_bytes(std::span{storage}).first(r.views_bytes),v),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<uint8_t> output(112+((input.size()+frame-1)/frame)*r.serialized_bytes);
    std::array<std::byte,112> header{};
    EXPECT_TRUE(serialize_lzss_position_distance_1m_stream_header(stream,limits,header));
    std::memcpy(output.data(),header.data(),header.size());
    std::size_t written=112,pos=0;std::uint64_t sequence=0;
    while(pos<input.size()) {
        const auto n=std::min<std::size_t>(frame,input.size()-pos);
        const auto result=encode_lzss_position_distance_1m_raw_frame(stream,limits,sequence++,pos,
            std::as_bytes(std::span{input}).subspan(pos,n),3,
            frame==21 ? marc::dictionary::internal::LzssPositionDistance1mSearch::exhaustive
                : marc::dictionary::internal::LzssPositionDistance1mSearch::indexed,
            v.tokens,v.operations,v.finder,v.serialized);
        EXPECT_EQ(result.error,LzssPositionDistanceRawFrameError::none);
        std::memcpy(output.data()+written,v.serialized.data(),result.frame.serialized_size);
        written+=result.frame.serialized_size;pos+=n;
    }
    output.resize(written);return output;
}
TEST(PositionDistance1mCFactory, ChunkedBytesMatchPrivateOracleAndRoundTrip) {
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
TEST(PositionDistance1mCFactory, CompletionEverySingleByteAndIndependentBufferSizes) {
    for (unsigned value=0; value<256; ++value) {
        SCOPED_TRACE(value);
        const std::vector<uint8_t> input{static_cast<uint8_t>(value)};
        const auto expected=oracle(input);
        EXPECT_EQ(run(config_for(MARC_DIRECTION_ENCODE,1),input,1,7),expected);
        EXPECT_EQ(run(config_for(MARC_DIRECTION_DECODE),expected,7,1),input);
    }
}
TEST(PositionDistance1mCFactory, CompletionDataClassesAndDeterministicChunking) {
    std::vector<std::vector<uint8_t>> inputs{{}, std::vector<uint8_t>(513,0),
        std::vector<uint8_t>(513,0xff), std::vector<uint8_t>(513),
        std::vector<uint8_t>(513), std::vector<uint8_t>(513)};
    uint32_t state=0x12345678u;
    for (std::size_t i=0;i<513;++i) {
        inputs[3][i]=static_cast<uint8_t>(i);
        inputs[4][i]=static_cast<uint8_t>(i%7);
        state^=state<<13; state^=state>>17; state^=state<<5;
        inputs[5][i]=static_cast<uint8_t>(state);
    }
    // Already-coded bytes are a separate binary input class, not a ratio assertion.
    inputs.push_back(oracle(inputs.back()));
    for (std::size_t index=0;index<inputs.size();++index) {
        SCOPED_TRACE(index);
        const auto& input=inputs[index];
        const auto expected=oracle(input);
        for (const auto chunks : {std::array<std::size_t,2>{1,7}, {7,1}, {13,29}, {4096,4096}}) {
            SCOPED_TRACE(chunks[0]);
            SCOPED_TRACE(chunks[1]);
            EXPECT_EQ(run(config_for(MARC_DIRECTION_ENCODE,input.size()),input,chunks[0],chunks[1]),expected);
            EXPECT_EQ(run(config_for(MARC_DIRECTION_DECODE),expected,chunks[1],chunks[0]),input);
        }
    }
}
TEST(PositionDistance1mCFactory, CompletionDefaultFrameAndMatchBoundaries) {
    for (std::size_t size : {257u,258u,259u,1048575u,1048576u,1048577u}) {
        SCOPED_TRACE(size);
        std::vector<uint8_t> input(size);
        for (std::size_t i=0;i<size;++i) input[i]=static_cast<uint8_t>(i%7);
        Config encoder{},decoder{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_config_init(MARC_DIRECTION_ENCODE,&encoder),MARC_STATUS_OK);
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_config_init(MARC_DIRECTION_DECODE,&decoder),MARC_STATUS_OK);
        encoder.original_size=size;
        const auto encoded=run(encoder,input,4096,4096);
        EXPECT_EQ(run(encoder,input,257,31),encoded);
        EXPECT_EQ(run(decoder,encoded,31,257),input);
    }
}
TEST(PositionDistance1mCFactory, WorkspaceFailuresPublishNull) {
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
            EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,p,q,v,&t),MARC_STATUS_INVALID_ARGUMENT);
            EXPECT_EQ(t,nullptr);
        }
        marc_transform* t{};
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(nullptr,s.p(),s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    }
}
TEST(PositionDistance1mCFactory, MetadataAliasesAreRejectedWithoutWrites) {
    auto c=config_for(MARC_DIRECTION_ENCODE); Storage s(c);
    const auto before=c;
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),
        reinterpret_cast<marc_transform**>(&c)),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
    for (auto b : {s.p(),s.s(),s.v()}) {
        std::array<uint8_t,sizeof(marc_transform*)> saved{};
        std::memcpy(saved.data(),b.data,saved.size());
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),
            reinterpret_cast<marc_transform**>(b.data)),MARC_STATUS_INVALID_ARGUMENT);
        EXPECT_EQ(std::memcmp(saved.data(),b.data,saved.size()),0);
    }
    marc_transform* t{};
    EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,
        {reinterpret_cast<uint8_t*>(&c),s.r.primary_bytes},s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(t,nullptr); EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
}
TEST(PositionDistance1mCFactory, ExactAggregateIncludesHandleAndUnusedTailsAreFree) {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config_for(direction); Storage s(c);
        uint64_t low=c.max_block_size,high=c.max_internal_buffered_bytes;
        marc_workspace_requirements r{};
        while(low<high) {
            c.max_internal_buffered_bytes=low+(high-low)/2;
            if(marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(&c,&r)==MARC_STATUS_OK)
                high=c.max_internal_buffered_bytes;
            else low=c.max_internal_buffered_bytes+1;
        }
        c.max_internal_buffered_bytes=low;
        marc_transform* t{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
        marc_transform_destroy(t); s.tails();
        const auto empty_archive=oracle({});
        const auto output=run(c,direction==MARC_DIRECTION_ENCODE?std::vector<uint8_t>{}:empty_archive,1,1);
        EXPECT_EQ(output,direction==MARC_DIRECTION_ENCODE?empty_archive:std::vector<uint8_t>{});
        --c.max_internal_buffered_bytes;
        EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_EQ(t,nullptr);
    }
}
TEST(PositionDistance1mCFactory, SecondFrameFailurePreservesOnlyFirstFrame) {
    auto bytes=oracle(std::vector<uint8_t>(42,42));
    const auto second=oracle(std::vector<uint8_t>(21,42)).size();
    ASSERT_LT(second,bytes.size()); bytes[second]^=1;
    for (std::size_t step : {1u,512u}) {
        auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
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
TEST(PositionDistance1mCFactory, TruncationPayloadAndTrailingFailuresPublishOnlyValidatedFrames) {
    std::vector<uint8_t> raw(42);
    for(std::size_t i=0;i<raw.size();++i) raw[i]=static_cast<uint8_t>(i*71+i/11);
    const auto valid=oracle(raw);
    const auto second=oracle(std::vector<uint8_t>(raw.begin(),raw.begin()+21)).size();
    ASSERT_LT(second,valid.size());
    const auto check=[&](const std::vector<uint8_t>& bytes,std::size_t published,std::size_t position) {
        for(const auto schedule:{std::array<std::size_t,2>{1,1},{13,7},{512,31}})
        for(bool separate_end:{false,true}) {
            SCOPED_TRACE(bytes.size());
            SCOPED_TRACE(published);
            SCOPED_TRACE(schedule[0]);
            SCOPED_TRACE(separate_end);
            auto c=config_for(MARC_DIRECTION_DECODE); Storage storage(c); marc_transform* handle{};
            ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,storage.p(),storage.s(),storage.v(),
                &handle),MARC_STATUS_OK);
            std::unique_ptr<marc_transform,decltype(&marc_transform_destroy)> owner(handle,marc_transform_destroy);
            std::size_t consumed{}; std::vector<uint8_t> output;
            marc_process_result result{};
            for(std::size_t call=0;call<10000;++call) {
                const auto n=std::min(schedule[0],bytes.size()-consumed);
                const auto capacity=call%5==0?0:schedule[1];
                std::array<uint8_t,33> buffer; buffer.fill(0xcd);
                const auto end=separate_end?consumed==bytes.size():consumed+n==bytes.size();
                result=marc_transform_process(handle,{n?bytes.data()+consumed:nullptr,n},
                    {buffer.data()+1,capacity},MARC_PROCESS_FLUSH|(end?MARC_PROCESS_END_INPUT:0u));
                ASSERT_LE(result.input_consumed,n); ASSERT_LE(result.output_produced,capacity);
                EXPECT_EQ(buffer.front(),0xcd);
                EXPECT_TRUE(std::all_of(buffer.begin()+1+result.output_produced,buffer.end(),
                    [](auto b){return b==0xcd;}));
                EXPECT_FALSE(result.status==MARC_STATUS_PROGRESS && !result.input_consumed && !result.output_produced);
                consumed+=result.input_consumed;
                output.insert(output.end(),buffer.begin()+1,buffer.begin()+1+result.output_produced);
                if(result.status>=100 || result.status==MARC_STATUS_END_OF_STREAM) break;
            }
            EXPECT_EQ(result.status,MARC_STATUS_MALFORMED_STREAM);
            EXPECT_EQ(result.error_byte_position,position);
            EXPECT_EQ(result.error_bit_position,0u);
            EXPECT_EQ(output,std::vector<uint8_t>(raw.begin(),raw.begin()+published));
            for(auto flags:{MARC_PROCESS_NONE,MARC_PROCESS_END_INPUT,MARC_PROCESS_FLUSH}) {
                std::array<uint8_t,7> buffer; buffer.fill(0xcd);
                const auto again=marc_transform_process(handle,{nullptr,0},{buffer.data(),buffer.size()},flags);
                EXPECT_EQ(again.status,result.status); EXPECT_EQ(again.error_byte_position,result.error_byte_position);
                EXPECT_EQ(again.error_bit_position,result.error_bit_position);
                EXPECT_EQ(again.input_consumed,0u); EXPECT_EQ(again.output_produced,0u);
                EXPECT_TRUE(std::all_of(buffer.begin(),buffer.end(),[](auto b){return b==0xcd;}));
            }
            owner.reset(); storage.tails();
        }
    };
    for(std::size_t n=0;n<valid.size();++n)
        check(std::vector<uint8_t>(valid.begin(),valid.begin()+n),n>=second?21:0,n);
    auto trailing=valid; trailing.push_back(0x5a);
    check(trailing,raw.size(),valid.size());
    auto damaged=valid; damaged.back()^=0x80;
    using namespace marc::frame::internal;
    const TypedContextStreamHeader stream{21,raw.size(),{1048576,3,258,0},32768,44,9,1,10};
    std::array<marc::dictionary::internal::LzssTypedToken,21> tokens{};
    std::array<std::byte,21> restored; restored.fill(std::byte{0xcc});
    const auto reference=decode_lzss_position_distance_1m_frame(std::as_bytes(std::span{damaged}).subspan(second),
        {stream,{},1,21},tokens,restored);
    ASSERT_NE(reference.error,LzssShortMatchFrameDecodeError::none);
    ASSERT_EQ(reference.preflight_error,LzssShortMatchPreflightError::none);
    ASSERT_TRUE(std::all_of(restored.begin(),restored.end(),[](auto b){return b==std::byte{0xcc};}));
    check(damaged,21,second+80);
}
TEST(PositionDistance1mCFactory, MalformedInputAndUnsupportedResetAreSticky) {
    auto encoded=oracle(std::vector<uint8_t>(49,42));
    encoded[0]^=1;
    auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
    ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
    std::array<uint8_t,64> output{};
    auto result=marc_transform_process(t,{encoded.data(),encoded.size()},{output.data(),output.size()},MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status,MARC_STATUS_MALFORMED_STREAM); EXPECT_EQ(result.output_produced,0u);
    auto sticky=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE);
    EXPECT_EQ(sticky.status,result.status); EXPECT_EQ(sticky.error_byte_position,result.error_byte_position);
    marc_transform_destroy(t);
    c=config_for(MARC_DIRECTION_ENCODE); Storage e(c);
    ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,e.p(),e.s(),e.v(),&t),MARC_STATUS_OK);
    result=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_RESET_BLOCK);
    EXPECT_EQ(result.status,MARC_STATUS_UNSUPPORTED);
    EXPECT_EQ(marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE).status,result.status);
    marc_transform_destroy(t);
}
TEST(PositionDistance1mCFactory, AdmissionRejectsCrossedIdentitiesBeforeOutput) {
    const auto valid = oracle({});
    ASSERT_EQ(valid.size(), 112u);
    EXPECT_TRUE(run(config_for(MARC_DIRECTION_DECODE), valid, 1, 1).empty());
    std::vector<std::vector<uint8_t>> rejected;
    for (uint8_t dictionary = 0; dictionary <= 9; ++dictionary) {
        for (uint8_t context = 0; context <= 10; ++context) {
            if (dictionary == 9 && context == 10) continue;
            auto bytes = valid;
            bytes[14] = dictionary;
            bytes[98] = context;
            rejected.push_back(bytes);
        }
    }
    // Alter each remaining identity word, including unknown high-byte values.
    for (std::size_t offset : {4u, 6u, 12u, 16u, 18u, 96u}) {
        auto bytes = valid;
        bytes[offset] ^= 1;
        rejected.push_back(bytes);
    }
    for (std::size_t offset : {4u, 6u, 12u, 14u, 16u, 18u, 96u, 98u}) {
        auto bytes = valid;
        bytes[offset + 1] = 0xff;
        rejected.push_back(bytes);
    }
    for (std::size_t index = 0; index < rejected.size(); ++index) {
        SCOPED_TRACE(index);
        for (std::size_t chunk : {1u, 112u}) {
            SCOPED_TRACE(chunk);
            const auto c = config_for(MARC_DIRECTION_DECODE);
            Storage storage(c);
            marc_transform* transform{};
            ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(
                &c, storage.p(), storage.s(), storage.v(), &transform), MARC_STATUS_OK);
            std::array<uint8_t, 16> output;
            output.fill(0xcd);
            std::size_t consumed{};
            marc_process_result result{};
            for (std::size_t call = 0; call <= valid.size(); ++call) {
                const auto n = std::min(chunk, valid.size() - consumed);
                result = marc_transform_process(transform,
                    {rejected[index].data() + consumed, n},
                    {output.data(), output.size()},
                    consumed + n == valid.size() ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE);
                EXPECT_LE(result.input_consumed, n);
                EXPECT_EQ(result.output_produced, 0u);
                if (result.input_consumed > n) break;
                consumed += result.input_consumed;
                if (result.status >= 100 || result.status == MARC_STATUS_END_OF_STREAM) break;
            }
            EXPECT_EQ(result.status, MARC_STATUS_UNSUPPORTED);
            EXPECT_EQ(result.error_byte_position, 0u);
            const auto sticky = marc_transform_process(transform, {nullptr, 0},
                {output.data(), output.size()}, MARC_PROCESS_END_INPUT);
            EXPECT_EQ(sticky.status, result.status);
            EXPECT_EQ(sticky.input_consumed, 0u);
            EXPECT_EQ(sticky.output_produced, 0u);
            EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto b) { return b == 0xcd; }));
            marc_transform_destroy(transform);
            storage.tails();
        }
    }
}

TEST(PositionDistance1mCFactory, LegacyFieldContextParserRemainsNarrow) {
    const auto bytes = oracle({});
    marc::frame::internal::TypedContextStreamHeader parsed{};
    parsed.original_size = 123;
    std::size_t consumed = 7;
    EXPECT_EQ(marc::frame::internal::parse_typed_context_stream_header(
        std::as_bytes(std::span{bytes}), marc::core::DecoderLimits{}, parsed, consumed),
        marc::frame::internal::TypedContextStreamHeaderError::unsupported_dictionary_variant);
    EXPECT_EQ(parsed.original_size, 123u);
    EXPECT_EQ(consumed, 7u);
}

TEST(PositionDistance1mCFactory, FivePrefixPublicLayoutAndWideReferenceIdentity) {
    using namespace marc::frame::internal;
    for(uint32_t frame:{1U,2U,3U,5U,1048576U}) {
        auto c=config_for(MARC_DIRECTION_ENCODE,frame);
        c.frame_size=c.max_frame_size=c.max_block_size=frame;
        c.max_compressed_payload_size=18ULL*frame+5;
        Storage storage(c);
        const TypedContextStreamHeader stream{frame,frame,{1048576,3,258,0},32768,44,9,1,10};
        LzssPositionDistanceWorkspaceRequirements old{};
        ASSERT_EQ(calculate_lzss_position_distance_1m_encode_workspace(stream,{},
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance1mFrameStreamingEncoder),old),
            LzssPositionDistanceWorkspaceError::none);
        EXPECT_EQ(storage.r.primary_bytes,old.raw_bytes);
        EXPECT_EQ(storage.r.secondary_bytes,old.serialized_bytes);
        const auto extra=frame<3?0U:4U*(65536+frame);
        EXPECT_EQ(storage.r.views_bytes,old.views_bytes+extra);
        if(extra) {
            auto short_views=storage.v();short_views.size=old.views_bytes;
            marc_transform* transform{};
            EXPECT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(&c,storage.p(),storage.s(),
                short_views,&transform),MARC_STATUS_INVALID_ARGUMENT);
            EXPECT_EQ(transform,nullptr);
            const auto view=storage.v();
            EXPECT_TRUE(std::all_of(view.data,view.data+view.size,[](auto b){return b==0xcd;}));
        }
    }
    std::vector<uint8_t> input(1048581);uint32_t seed=719;
    for(std::size_t i=0;i<70001;++i) {seed=seed*1664525U+1013904223U;input[i]=static_cast<uint8_t>(seed>>24);}
    for(std::size_t i=70001;i<input.size();++i) input[i]=input[i%70001];
    Config c{};ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_config_init(MARC_DIRECTION_ENCODE,&c),MARC_STATUS_OK);
    c.original_size=input.size();
    const auto expected=oracle(input,1048576);
    EXPECT_EQ(run(c,input,8191,4093),expected);
    EXPECT_EQ(run(c,input,65536,65536),expected);
    Config d{};ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_config_init(MARC_DIRECTION_DECODE,&d),MARC_STATUS_OK);
    EXPECT_EQ(run(d,expected,103,997),input);
}
TEST(PositionDistance1mCFactory, EncoderSecondFrameFailureKeepsPreparedFramePrivate) {
    std::vector<uint8_t> input(512, 'a');
    for (std::size_t i=0; i<256; ++i) input[i]=static_cast<uint8_t>(i);
    const auto expected=oracle(input,256);
    const auto first_size=oracle(std::vector<uint8_t>(input.begin(),input.begin()+256),256).size();
    for (const auto capacity : {std::size_t{1},std::size_t{16384}}) {
        auto c=config_for(MARC_DIRECTION_ENCODE,input.size());
        c.frame_size=256; c.max_frame_size=256; c.max_block_size=256;
        c.max_compressed_payload_size=18*256+5;
        c.max_expansion_ratio=1; c.expansion_slack=0;
        Storage storage(c); marc_transform* transform{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_1m_create(
            &c,storage.p(),storage.s(),storage.v(),&transform),MARC_STATUS_OK);
        std::vector<uint8_t> buffer(capacity,0xa5),published;
        std::size_t consumed{}; marc_process_result result{};
        for (unsigned call=0; call<20000; ++call) {
            std::fill(buffer.begin(),buffer.end(),0xa5);
            result=marc_transform_process(transform,
                {input.data()+consumed,input.size()-consumed},
                {buffer.data(),buffer.size()},MARC_PROCESS_END_INPUT);
            ASSERT_LE(result.input_consumed,input.size()-consumed);
            ASSERT_LE(result.output_produced,buffer.size());
            consumed+=result.input_consumed;
            published.insert(published.end(),buffer.begin(),buffer.begin()+result.output_produced);
            EXPECT_TRUE(std::all_of(buffer.begin()+result.output_produced,buffer.end(),
                [](auto value){return value==0xa5;}));
            if (result.status>=100 || result.status==MARC_STATUS_END_OF_STREAM) break;
        }
        EXPECT_EQ(result.status,MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_EQ(result.error_byte_position,256u); EXPECT_EQ(consumed,input.size());
        ASSERT_EQ(published.size(),first_size);
        EXPECT_TRUE(std::equal(published.begin(),published.end(),expected.begin()));
        const auto again=marc_transform_process(transform,{nullptr,0},
            {buffer.data(),buffer.size()},MARC_PROCESS_END_INPUT);
        EXPECT_EQ(again.status,result.status); EXPECT_EQ(again.error_byte_position,256u);
        EXPECT_EQ(again.input_consumed,0u); EXPECT_EQ(again.output_produced,0u);
        marc_transform_destroy(transform); storage.tails();
    }
}
} // namespace

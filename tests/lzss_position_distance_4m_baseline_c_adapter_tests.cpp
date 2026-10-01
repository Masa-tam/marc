#include "frame/lzss_position_distance_4m_baseline_c_adapter.h"
#include "frame/lzss_position_distance_4m_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
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
using Config = marc_private_position_distance_4m_config;
Config config_for(marc_direction direction, std::size_t size = 0) {
    Config c{};
    EXPECT_EQ(marc_private_position_distance_4m_baseline_config_init(direction, &c), MARC_STATUS_OK);
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
        EXPECT_EQ(marc_private_position_distance_4m_baseline_workspace_requirements(&c, &r), MARC_STATUS_OK);
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
    EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),&t), MARC_STATUS_OK);
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
    const TypedContextStreamHeader stream{frame,input.size(),{4194304,3,258,0},32768,46,10,1,11};
    marc::core::DecoderLimits limits{};
    limits.max_block_size=4194304;limits.max_compressed_payload_size=75497477;limits.max_internal_buffered_bytes=512U<<20;
    LzssPositionDistanceWorkspaceRequirements r{};
    EXPECT_EQ(calculate_lzss_position_distance_4m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance4mFrameStreamingEncoder),r),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> raw(r.raw_bytes), serialized(r.serialized_bytes);
    std::vector<std::max_align_t> storage((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    LzssPositionDistanceWorkspaceViews v{};
    EXPECT_EQ(partition_lzss_position_distance_4m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance4mFrameStreamingEncoder),
        raw,serialized,std::as_writable_bytes(std::span{storage}).first(r.views_bytes),v),
        LzssPositionDistanceWorkspaceError::none);
    std::vector<uint8_t> output(112+((input.size()+frame-1)/frame)*r.serialized_bytes);
    std::array<std::byte,112> header{};
    EXPECT_TRUE(serialize_lzss_position_distance_4m_stream_header(stream,limits,header));
    std::memcpy(output.data(),header.data(),header.size());
    std::size_t written=112,pos=0;std::uint64_t sequence=0;
    while(pos<input.size()) {
        const auto n=std::min<std::size_t>(frame,input.size()-pos);
        const auto result=encode_lzss_position_distance_4m_raw_frame(stream,limits,sequence++,pos,
            std::as_bytes(std::span{input}).subspan(pos,n),3,
            frame==21 ? marc::dictionary::internal::LzssPositionDistance4mSearch::exhaustive
                : marc::dictionary::internal::LzssPositionDistance4mSearch::indexed_reference,
            v.tokens,v.operations,v.finder,v.serialized);
        EXPECT_EQ(result.error,LzssPositionDistanceRawFrameError::none);
        std::memcpy(output.data()+written,v.serialized.data(),result.frame.serialized_size);
        written+=result.frame.serialized_size;pos+=n;
    }
    output.resize(written);return output;
}
TEST(PositionDistance4mBaselineCFactory, ChunkedBytesMatchPrivateOracleAndRoundTrip) {
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
TEST(PositionDistance4mBaselineCFactory, CompletionEverySingleByteAndIndependentBufferSizes) {
    for (unsigned value=0; value<256; ++value) {
        SCOPED_TRACE(value);
        const std::vector<uint8_t> input{static_cast<uint8_t>(value)};
        const auto expected=oracle(input);
        EXPECT_EQ(run(config_for(MARC_DIRECTION_ENCODE,1),input,1,7),expected);
        EXPECT_EQ(run(config_for(MARC_DIRECTION_DECODE),expected,7,1),input);
    }
}
TEST(PositionDistance4mBaselineCFactory, CompletionDataClassesAndDeterministicChunking) {
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
TEST(PositionDistance4mBaselineCFactory, CompletionDefaultFrameAndMatchBoundaries) {
    for (std::size_t size : {257u,258u,259u,1048575u,4194304u,1048577u}) {
        SCOPED_TRACE(size);
        std::vector<uint8_t> input(size);
        for (std::size_t i=0;i<size;++i) input[i]=static_cast<uint8_t>(i%7);
        Config encoder{},decoder{};
        ASSERT_EQ(marc_private_position_distance_4m_baseline_config_init(MARC_DIRECTION_ENCODE,&encoder),MARC_STATUS_OK);
        ASSERT_EQ(marc_private_position_distance_4m_baseline_config_init(MARC_DIRECTION_DECODE,&decoder),MARC_STATUS_OK);
        encoder.original_size=size;
        const auto encoded=run(encoder,input,4096,4096);
        EXPECT_EQ(run(encoder,input,257,31),encoded);
        EXPECT_EQ(run(decoder,encoded,31,257),input);
    }
}
TEST(PositionDistance4mBaselineCFactory, WorkspaceFailuresPublishNull) {
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
            EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,p,q,v,&t),MARC_STATUS_INVALID_ARGUMENT);
            EXPECT_EQ(t,nullptr);
        }
        marc_transform* t{};
        EXPECT_EQ(marc_private_position_distance_4m_baseline_create(nullptr,s.p(),s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    }
}
TEST(PositionDistance4mBaselineCFactory, MetadataAliasesAreRejectedWithoutWrites) {
    auto c=config_for(MARC_DIRECTION_ENCODE); Storage s(c);
    const auto before=c;
    EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),
        reinterpret_cast<marc_transform**>(&c)),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
    for (auto b : {s.p(),s.s(),s.v()}) {
        std::array<uint8_t,sizeof(marc_transform*)> saved{};
        std::memcpy(saved.data(),b.data,saved.size());
        EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),
            reinterpret_cast<marc_transform**>(b.data)),MARC_STATUS_INVALID_ARGUMENT);
        EXPECT_EQ(std::memcmp(saved.data(),b.data,saved.size()),0);
    }
    marc_transform* t{};
    EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,
        {reinterpret_cast<uint8_t*>(&c),s.r.primary_bytes},s.s(),s.v(),&t),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(t,nullptr); EXPECT_EQ(std::memcmp(&before,&c,sizeof(c)),0);
}
TEST(PositionDistance4mBaselineCFactory, SecondFrameFailurePreservesOnlyFirstFrame) {
    auto bytes=oracle(std::vector<uint8_t>(42,42));
    const auto second=oracle(std::vector<uint8_t>(21,42)).size();
    ASSERT_LT(second,bytes.size()); bytes[second]^=1;
    for (std::size_t step : {1u,512u}) {
        auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
        ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
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
TEST(PositionDistance4mBaselineCFactory, TruncationPayloadAndTrailingFailuresPublishOnlyValidatedFrames) {
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
            ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,storage.p(),storage.s(),storage.v(),
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
    const TypedContextStreamHeader stream{21,raw.size(),{4194304,3,258,0},32768,46,10,1,11};
    std::array<marc::dictionary::internal::LzssTypedToken,21> tokens{};
    std::array<std::byte,21> restored; restored.fill(std::byte{0xcc});
    const auto reference=decode_lzss_position_distance_4m_frame(std::as_bytes(std::span{damaged}).subspan(second),
        {stream,{},1,21},tokens,restored);
    ASSERT_NE(reference.error,LzssShortMatchFrameDecodeError::none);
    ASSERT_EQ(reference.preflight_error,LzssShortMatchPreflightError::none);
    ASSERT_TRUE(std::all_of(restored.begin(),restored.end(),[](auto b){return b==std::byte{0xcc};}));
    check(damaged,21,second+80);
}
TEST(PositionDistance4mBaselineCFactory, MalformedInputAndUnsupportedResetAreSticky) {
    auto encoded=oracle(std::vector<uint8_t>(49,42));
    encoded[0]^=1;
    auto c=config_for(MARC_DIRECTION_DECODE); Storage s(c); marc_transform* t{};
    ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),&t),MARC_STATUS_OK);
    std::array<uint8_t,64> output{};
    auto result=marc_transform_process(t,{encoded.data(),encoded.size()},{output.data(),output.size()},MARC_PROCESS_END_INPUT);
    EXPECT_EQ(result.status,MARC_STATUS_MALFORMED_STREAM); EXPECT_EQ(result.output_produced,0u);
    auto sticky=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE);
    EXPECT_EQ(sticky.status,result.status); EXPECT_EQ(sticky.error_byte_position,result.error_byte_position);
    marc_transform_destroy(t);
    c=config_for(MARC_DIRECTION_ENCODE); Storage e(c);
    ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,e.p(),e.s(),e.v(),&t),MARC_STATUS_OK);
    result=marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_RESET_BLOCK);
    EXPECT_EQ(result.status,MARC_STATUS_UNSUPPORTED);
    EXPECT_EQ(marc_transform_process(t,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE).status,result.status);
    marc_transform_destroy(t);
}
uint64_t minimum_budget(Config c) {
    uint64_t low=1,high=c.max_internal_buffered_bytes;
    while(low<high) {
        const auto middle=low+(high-low)/2;c.max_internal_buffered_bytes=middle;
        marc_workspace_requirements r{};
        if(marc_private_position_distance_4m_baseline_workspace_requirements(&c,&r)==MARC_STATUS_OK) high=middle;
        else low=middle+1;
    }
    return low;
}
TEST(PositionDistance4mBaselineCFactory, ExactAggregateAndFullTailCapacitiesAreCharged) {
    for(auto direction:{MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config_for(direction,1);const auto minimum=minimum_budget(c);
        c.max_internal_buffered_bytes=minimum;Storage s(c);
        const marc_buffer p{s.p().data,s.r.primary_bytes},q{s.s().data,s.r.secondary_bytes},v{s.v().data,s.r.views_bytes};
        marc_transform* handle{};
        ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,p,q,v,&handle),MARC_STATUS_OK);
        marc_transform_destroy(handle);
        Storage untouched(c);handle=reinterpret_cast<marc_transform*>(1);
        --c.max_internal_buffered_bytes;
        auto query=s.r;const auto saved=query;
        EXPECT_EQ(marc_private_position_distance_4m_baseline_workspace_requirements(&c,&query),MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_EQ(std::memcmp(&query,&saved,sizeof(query)),0);
        EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,p,q,v,&handle),MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_EQ(handle,nullptr);
        c.max_internal_buffered_bytes=minimum;
        const auto extra=(untouched.p().size-untouched.r.primary_bytes)+(untouched.s().size-untouched.r.secondary_bytes)+(untouched.v().size-untouched.r.views_bytes);
        c.max_internal_buffered_bytes+=extra-1;
        EXPECT_EQ(marc_private_position_distance_4m_baseline_create(&c,untouched.p(),untouched.s(),untouched.v(),&handle),MARC_STATUS_LIMIT_EXCEEDED);
        EXPECT_TRUE(std::all_of(untouched.primary.begin(),untouched.primary.end(),[](auto b){return b==0xcd;}));
        EXPECT_TRUE(std::all_of(untouched.secondary.begin(),untouched.secondary.end(),[](auto b){return b==0xcd;}));
        auto view=untouched.v();EXPECT_TRUE(std::all_of(view.data,view.data+view.size,[](auto b){return b==0xcd;}));
        ++c.max_internal_buffered_bytes;
        ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,untouched.p(),untouched.s(),untouched.v(),&handle),MARC_STATUS_OK);
        marc_transform_destroy(handle);untouched.tails();
    }
}
TEST(PositionDistance4mBaselineCFactory, ProcessGuardsHandleAndEveryUnusedTailWithoutWrites) {
    for(auto direction:{MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE})
    for(unsigned choice=0;choice<5;++choice) {
        auto c=config_for(direction,1);Storage s(c);marc_transform* handle{};
        ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),&handle),MARC_STATUS_OK);
        std::array<uint8_t,sizeof(void*)> saved{};std::memcpy(saved.data(),handle,saved.size());
        uint8_t* pointer=choice<2?reinterpret_cast<uint8_t*>(handle):choice==2?s.p().data+s.r.primary_bytes:
            choice==3?s.s().data+s.r.secondary_bytes:s.v().data+s.r.views_bytes;
        const auto result=marc_transform_process(handle,choice==0?marc_const_buffer{nullptr,0}:marc_const_buffer{pointer,1},
            choice==0?marc_buffer{pointer,sizeof(void*)}:marc_buffer{nullptr,0},0);
        EXPECT_EQ(result.status,MARC_STATUS_INVALID_ARGUMENT);EXPECT_EQ(result.input_consumed,0);EXPECT_EQ(result.output_produced,0);
        EXPECT_EQ(std::memcmp(saved.data(),handle,saved.size()),0);
        const auto again=marc_transform_process(handle,{nullptr,0},{nullptr,0},0);
        EXPECT_EQ(again.status,result.status);EXPECT_EQ(again.input_consumed,0);EXPECT_EQ(again.output_produced,0);
        marc_transform_destroy(handle);s.tails();
    }
}
TEST(PositionDistance4mBaselineCFactory, InvalidConfigAndQueryMetadataPreserveOutputs) {
    auto c=config_for(MARC_DIRECTION_ENCODE,1);Storage s(c);
    for(unsigned choice=0;choice<5;++choice) {
        auto bad=c;
        if(choice==0) --bad.struct_size;
        if(choice==1) ++bad.abi_version;
        if(choice==2) bad.reserved=1;
        if(choice==3) bad.reserved2=1;
        if(choice==4) bad.frame_size=4194305;
        auto result=s.r;const auto saved=result;
        EXPECT_EQ(marc_private_position_distance_4m_baseline_workspace_requirements(&bad,&result),MARC_STATUS_INVALID_ARGUMENT);
        EXPECT_EQ(std::memcmp(&saved,&result,sizeof(result)),0);
    }
    const auto saved=c;
    EXPECT_EQ(marc_private_position_distance_4m_baseline_workspace_requirements(&c,reinterpret_cast<marc_workspace_requirements*>(&c)),MARC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&saved,&c,sizeof(c)),0);
}
TEST(PositionDistance4mBaselineCFactory, FullWindowFinalSuffixMatchesScalarAndRestores) {
    const std::vector<uint8_t> input(4194305,'a');
    const auto expected=oracle(input,4194304);
    Config c{},d{};
    ASSERT_EQ(marc_private_position_distance_4m_baseline_config_init(MARC_DIRECTION_ENCODE,&c),MARC_STATUS_OK);
    ASSERT_EQ(marc_private_position_distance_4m_baseline_config_init(MARC_DIRECTION_DECODE,&d),MARC_STATUS_OK);
    c.original_size=input.size();
    EXPECT_EQ(run(c,input,8191,4093),expected);
    EXPECT_EQ(run(d,expected,103,997),input);
}
TEST(PositionDistance4mBaselineCFactory, CrossedWireIdentityIsRejectedBeforeRawPublication) {
    const auto valid=oracle(std::vector<uint8_t>{'a'});
    for(auto tuple:std::array<std::array<uint8_t,2>,3>{{{{9,10}},{{10,10}},{{9,11}}}}) {
        auto bytes=valid;bytes[14]=tuple[0];bytes[98]=tuple[1];
        auto c=config_for(MARC_DIRECTION_DECODE);Storage s(c);marc_transform* handle{};
        ASSERT_EQ(marc_private_position_distance_4m_baseline_create(&c,s.p(),s.s(),s.v(),&handle),MARC_STATUS_OK);
        std::array<uint8_t,64> output{};output.fill(0xa5);
        const auto result=marc_transform_process(handle,{bytes.data(),bytes.size()},{output.data(),output.size()},MARC_PROCESS_END_INPUT);
        EXPECT_GE(result.status,100);EXPECT_EQ(result.output_produced,0);
        EXPECT_TRUE(std::all_of(output.begin(),output.end(),[](auto b){return b==0xa5;}));
        const auto again=marc_transform_process(handle,{nullptr,0},{output.data(),output.size()},0);
        EXPECT_EQ(again.status,result.status);EXPECT_EQ(again.error_byte_position,result.error_byte_position);
        marc_transform_destroy(handle);s.tails();
    }
}
} // namespace

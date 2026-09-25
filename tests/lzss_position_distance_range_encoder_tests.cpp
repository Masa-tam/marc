#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_decoder.hpp"
#include "context/lzss_position_distance_field_cursor.hpp"
#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using Error = ContextualDynamicRangeEncodeError;

std::vector<ModeledOperation> mixed() {
    constexpr std::array<unsigned,14> values{0,97,0,98,1,8,1,1,1,1,8,0,1,0};
    LzssPositionDistanceFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    for (auto value : values) {
        auto op=cursor.next().shape; op.value=value;
        EXPECT_EQ(cursor.accept(op),LzssFieldContextError::none);
        ops.push_back(op);
    }
    return ops;
}

TEST(LzssPositionDistanceRangeEncoder, IndependentMixedVector) {
    const auto ops=mixed();
    constexpr std::array<unsigned,9> expected{0,48,152,190,146,107,61,34,142};
    std::array<std::byte,10> output; output.fill(std::byte{0x55});
    ContextualDynamicRangeDescriptor descriptor{};
    const auto result=encode_lzss_position_distance_range_operations(ops,{},output,descriptor);
    ASSERT_EQ(result.error,Error::none);
    EXPECT_EQ(result.operation_count,14); EXPECT_EQ(result.decision_count,14);
    EXPECT_EQ(descriptor.context_count,40); EXPECT_EQ(result.payload_size,9);
    for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(output[i],std::byte(expected[i]));
    EXPECT_EQ(output.back(),std::byte{0x55});
}

TEST(LzssPositionDistanceRangeEncoder, WideDistanceFixedVectorAndRoundTrip) {
    LzssPositionDistanceFieldCursor cursor;
    std::vector<ModeledOperation> ops;
    auto append=[&](unsigned value) {
        auto op=cursor.next().shape; op.value=value;
        EXPECT_EQ(cursor.accept(op),LzssFieldContextError::none); ops.push_back(op);
    };
    for (unsigned i=0;i<16;++i) { append(0); append(97); }
    for (auto v : {1U,0U,3U,5U}) append(v);
    constexpr std::array<unsigned,18> expected{0,48,152,79,209,96,9,207,77,61,39,231,140,67,173,72,11,64};
    std::array<std::byte,18> output{};
    ContextualDynamicRangeDescriptor descriptor{};
    auto result=encode_lzss_position_distance_range_operations(ops,{},output,descriptor);
    ASSERT_EQ(result.error,Error::none);
    EXPECT_EQ(result.decision_count,38); EXPECT_EQ(result.operation_count,36);
    for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(output[i],std::byte(expected[i]));
    LzssPositionDistanceRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor,output,{}).error,ContextualDynamicRangeDecodeError::none);
    for (const auto& expected_op : ops) {
        ModeledOperation actual{};
        ASSERT_EQ(decoder.decode_next(actual).error,ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(actual.value,expected_op.value); EXPECT_EQ(actual.kind,expected_op.kind);
        EXPECT_EQ(actual.context_id,expected_op.context_id); EXPECT_EQ(actual.bit_count,expected_op.bit_count);
    }
    EXPECT_EQ(decoder.finish(36,38).error,ContextualDynamicRangeDecodeError::none);
}

TEST(LzssPositionDistanceRangeEncoder, InvalidGrammarNeverWrites) {
    const auto valid=mixed();
    for (unsigned scenario=0;scenario<7;++scenario) {
        auto ops=valid;
        switch (scenario) {
        case 0: ops.pop_back(); break;
        case 1: ops[0].context_id=24; break;
        case 2: ops[6].bit_count=32; break;
        case 3: ops[6].value=2; break;
        case 4: ops[7].context_id=15; break;
        case 5: ops[5].value=7; ops[6].bit_count=7; ops[6].value=127; break;
        case 6: ops[7].value=16; ops[8].bit_count=16; ops[8].value=1; break;
        }
        std::array<std::byte,100> output; output.fill(std::byte{0x55});
        ContextualDynamicRangeDescriptor descriptor{99,99,99};
        EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},output,descriptor).error,Error::invalid_symbol);
        EXPECT_EQ(descriptor.context_count,99);
        for (auto b : output) EXPECT_EQ(b,std::byte{0x55});
    }
}

TEST(LzssPositionDistanceRangeEncoder, CapacityLimitsAndOverlapAreTransactional) {
    auto ops=mixed();
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    std::array<std::byte,9> output; output.fill(std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},std::span{output}.first(8),descriptor).error,Error::payload_output_too_small);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=ops.size()*sizeof(ModeledOperation)+lzss_position_distance_range_encoder_state_bytes()+9;
    auto tight=limits; --tight.max_internal_buffered_bytes;
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,tight,output,descriptor).error,Error::limit_exceeded);
    tight={}; tight.max_entropy_table_entries=2521;
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,tight,output,descriptor).error,Error::limit_exceeded);
    auto alias=std::as_writable_bytes(std::span{ops});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,{},alias,descriptor).error,Error::overlapping_buffers);
    EXPECT_EQ(descriptor.context_count,99);
    for (auto b : output) EXPECT_EQ(b,std::byte{0x55});
    EXPECT_EQ(encode_lzss_position_distance_range_operations(ops,limits,output,descriptor).error,Error::none);
}
} // namespace

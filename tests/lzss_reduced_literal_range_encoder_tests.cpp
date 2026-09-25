#include "entropy/lzss_reduced_literal_range_encoder.hpp"
#include "entropy/lzss_reduced_literal_range_decoder.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using Error = ContextualDynamicRangeEncodeError;
constexpr std::array operations{
    ModeledOperation{ModeledOperationKind::symbol,0,2,0,0},
    ModeledOperation{ModeledOperationKind::symbol,3,256,97,0},
    ModeledOperation{ModeledOperationKind::symbol,1,2,1,0},
    ModeledOperation{ModeledOperationKind::symbol,13,9,0,0},
    ModeledOperation{ModeledOperationKind::symbol,15,17,0,0}};
constexpr std::array expected{std::byte{0},std::byte{0x30},std::byte{0xbf},
    std::byte{0xff},std::byte{0x9e},std::byte{0x80},std::byte{0}};

TEST(LzssReducedLiteralRangeEncoder, ReproducesHandVectorAndExactExtent) {
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan=plan_lzss_reduced_literal_range_operations(operations,{},descriptor);
    ASSERT_EQ(plan.error,Error::none);
    EXPECT_EQ(plan.payload_size,7); EXPECT_EQ(plan.decision_count,5);
    EXPECT_EQ(descriptor.context_count,24);
    std::array<std::byte,8> payload{}; payload.fill(std::byte{0xcc});
    ASSERT_EQ(encode_lzss_reduced_literal_range_operations(operations,{},payload,descriptor).error,Error::none);
    for (std::size_t i=0;i<expected.size();++i) EXPECT_EQ(payload[i],expected[i]);
    EXPECT_EQ(payload.back(),std::byte{0xcc});
    EXPECT_EQ(descriptor.payload_size,7); EXPECT_EQ(descriptor.decision_count,5);
}

TEST(LzssReducedLiteralRangeEncoder, MatchesRelabeledReferenceAndRoundTripsRescaling) {
    std::vector<ModeledOperation> ops;
    for (std::uint16_t id=0;id<24;++id) {
        const auto alphabet=lzss_reduced_literal_alphabets[id];
        for (std::uint32_t v=0;v<alphabet;++v)
            ops.push_back({ModeledOperationKind::symbol,id,alphabet,v,0});
    }
    for (unsigned i=0;i<40000;++i)
        ops.push_back({ModeledOperationKind::symbol,5,256,i%3,0});
    for (std::uint8_t width=1;width<=16;++width)
        ops.push_back({ModeledOperationKind::bypass_bits,0,0,
            ((UINT32_C(1)<<width)-1)&UINT32_C(0x5555),width});
    auto old_ops=ops;
    for (auto& op:old_ops) if (op.kind==ModeledOperationKind::symbol && op.context_id>=12)
        op.context_id+=8;
    ContextualDynamicRangeDescriptor descriptor{}, old_descriptor{};
    const auto plan=plan_lzss_reduced_literal_range_operations(ops,{},descriptor);
    ASSERT_EQ(plan.error,Error::none);
    const auto old_plan=plan_lzss_short_match_range_operations(old_ops,{},old_descriptor);
    ASSERT_EQ(old_plan.error,Error::none);
    ASSERT_EQ(plan.payload_size,old_plan.payload_size);
    std::vector<std::byte> payload(plan.payload_size), old_payload(old_plan.payload_size);
    ASSERT_EQ(encode_lzss_reduced_literal_range_operations(ops,{},payload,descriptor).error,Error::none);
    ASSERT_EQ(encode_lzss_short_match_range_operations(old_ops,{},old_payload,old_descriptor).error,Error::none);
    EXPECT_EQ(payload,old_payload);
    LzssReducedLiteralRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor,payload,{}).error,ContextualDynamicRangeDecodeError::none);
    for (const auto& op:ops) {
        std::uint32_t value=UINT32_MAX;
        const auto decoded=op.kind==ModeledOperationKind::symbol
            ? decoder.decode_symbol(op.context_id,op.alphabet_size,value)
            : decoder.decode_bypass(op.bit_count,value);
        ASSERT_EQ(decoded.error,ContextualDynamicRangeDecodeError::none);
        ASSERT_EQ(value,op.value);
    }
    EXPECT_EQ(decoder.finish(static_cast<std::uint32_t>(ops.size()),descriptor.decision_count).error,
        ContextualDynamicRangeDecodeError::none);
}

TEST(LzssReducedLiteralRangeEncoder, RejectsMalformedOperationsWithoutPublication) {
    const std::array invalid{
        ModeledOperation{ModeledOperationKind::symbol,24,2,0,0},
        ModeledOperation{ModeledOperationKind::symbol,12,256,0,0},
        ModeledOperation{ModeledOperationKind::symbol,12,9,9,0},
        ModeledOperation{ModeledOperationKind::symbol,12,9,0,1},
        ModeledOperation{ModeledOperationKind::bypass_bits,0,0,0,17},
        ModeledOperation{ModeledOperationKind::bypass_bits,0,0,2,1},
        ModeledOperation{static_cast<ModeledOperationKind>(255),0,0,0,0}};
    constexpr std::array errors{Error::invalid_context,Error::invalid_alphabet,Error::invalid_symbol,
        Error::nonzero_unused_field,Error::invalid_bypass_width,Error::nonzero_unused_field,Error::invalid_operation_kind};
    for (std::size_t i=0;i<invalid.size();++i) {
        auto ops=operations; ops.back()=invalid[i];
        std::array<std::byte,20> payload{}; payload.fill(std::byte{0xcc});
        ContextualDynamicRangeDescriptor descriptor{99,99,99};
        EXPECT_EQ(encode_lzss_reduced_literal_range_operations(ops,{},payload,descriptor).error,errors[i]);
        for (const auto byte:payload) EXPECT_EQ(byte,std::byte{0xcc});
        EXPECT_EQ(descriptor.context_count,99); EXPECT_EQ(descriptor.payload_size,99);
    }
    ContextualDynamicRangeDescriptor d{99,99,99};
    EXPECT_EQ(plan_lzss_reduced_literal_range_operations({}, {},d).error,Error::empty_operations);
    EXPECT_EQ(d.context_count,99);
}

TEST(LzssReducedLiteralRangeEncoder, RejectsSmallOrOverlappingOutputAndHonorsExactMemory) {
    auto ops=operations;
    ContextualDynamicRangeDescriptor descriptor{99,99,99};
    std::array<std::byte,6> small{}; small.fill(std::byte{0xcc});
    EXPECT_EQ(encode_lzss_reduced_literal_range_operations(ops,{},small,descriptor).error,Error::payload_output_too_small);
    for (const auto byte:small) EXPECT_EQ(byte,std::byte{0xcc});
    EXPECT_EQ(descriptor.context_count,99);
    EXPECT_EQ(encode_lzss_reduced_literal_range_operations(ops,{},std::as_writable_bytes(std::span{ops}),descriptor).error,
        Error::overlapping_buffers);
    EXPECT_EQ(ops.back().context_id,operations.back().context_id);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=1;
    limits.max_internal_buffered_bytes=sizeof(operations)+lzss_reduced_literal_range_encoder_state_bytes()+7;
    EXPECT_EQ(plan_lzss_reduced_literal_range_operations(ops,limits,descriptor).error,Error::none);
    --limits.max_internal_buffered_bytes;
    descriptor={99,99,99};
    EXPECT_EQ(plan_lzss_reduced_literal_range_operations(ops,limits,descriptor).error,Error::limit_exceeded);
    EXPECT_EQ(descriptor.context_count,99);
    limits={}; limits.max_entropy_table_entries=2489;
    EXPECT_EQ(plan_lzss_reduced_literal_range_operations(ops,limits,descriptor).error,Error::limit_exceeded);
    limits={}; limits.max_compressed_payload_size=6;
    EXPECT_EQ(plan_lzss_reduced_literal_range_operations(ops,limits,descriptor).error,Error::limit_exceeded);
}
} // namespace

#include "context/lzss_reduced_literal_operations.hpp"
#include "context/lzss_reduced_literal_context_layout.hpp"
#include "entropy/lzss_reduced_literal_range_encoder.hpp"
#include "entropy/lzss_reduced_literal_range_decoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
constexpr LzssParameters parameters{65536,3,258,0};
constexpr std::array tokens{
    LzssTypedToken{LzssTypedTokenKind::literal,0x1f,0,0},
    LzssTypedToken{LzssTypedTokenKind::literal,0x20,0,0},
    LzssTypedToken{LzssTypedTokenKind::match,0,1,3},
    LzssTypedToken{LzssTypedTokenKind::literal,0xff,0,0},
    LzssTypedToken{LzssTypedTokenKind::literal,0,0,0}};
constexpr LzssTypedFrameValidationContext frame{5,7,0};

TEST(LzssReducedLiteralOperations, MatchesHandDerivedStateAndEscapeVector) {
    std::array<ModeledOperation,13> ops{}; ops.back().value=777;
    const auto plan=plan_lzss_reduced_literal_operations(tokens,parameters,frame,{});
    ASSERT_EQ(plan.error,LzssFieldContextError::none);
    EXPECT_EQ(plan.operation_count,12); EXPECT_EQ(plan.decision_count,12);
    const auto result=model_lzss_reduced_literal_tokens(tokens,parameters,frame,{},ops);
    ASSERT_EQ(result.error,LzssFieldContextError::none);
    constexpr std::array<std::uint16_t,12> ids{0,3,1,4,1,13,0,23,2,5,1,11};
    constexpr std::array<std::uint32_t,12> values{0,31,0,32,1,8,0,0,0,255,0,0};
    constexpr std::array<std::uint16_t,12> alphabets{2,256,2,256,2,9,0,17,2,256,2,256};
    for(std::size_t i=0;i<12;++i) {
        EXPECT_EQ(ops[i].context_id,ids[i]); EXPECT_EQ(ops[i].value,values[i]);
        EXPECT_EQ(ops[i].alphabet_size,alphabets[i]);
        EXPECT_EQ(ops[i].bit_count,i==6 ? 1 : 0);
        EXPECT_EQ(ops[i].kind,i==6 ? ModeledOperationKind::bypass_bits : ModeledOperationKind::symbol);
    }
    EXPECT_EQ(ops.back().value,777);
}

TEST(LzssReducedLiteralOperations, CoversEveryPreviousLiteralAndResetsPerCall) {
    for (std::uint32_t byte=0;byte<256;++byte) {
        const std::array input{LzssTypedToken{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(byte),0,0},
            LzssTypedToken{LzssTypedTokenKind::literal,7,0,0}};
        std::array<ModeledOperation,4> ops{};
        ASSERT_EQ(model_lzss_reduced_literal_tokens(input,parameters,{2,2,0},{},ops).error,LzssFieldContextError::none);
        EXPECT_EQ(ops[1].context_id,3); EXPECT_EQ(ops[3].context_id,4+(byte>>5));
    }
}

TEST(LzssReducedLiteralOperations, RoundTripsModeledOperationsThroughNewRangeCoder) {
    std::array<ModeledOperation,12> ops{};
    ASSERT_EQ(model_lzss_reduced_literal_tokens(tokens,parameters,frame,{},ops).error,LzssFieldContextError::none);
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan=plan_lzss_reduced_literal_range_operations(ops,{},descriptor);
    ASSERT_EQ(plan.error,ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_reduced_literal_range_operations(ops,{},payload,descriptor).error,ContextualDynamicRangeEncodeError::none);
    LzssReducedLiteralRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor,payload,{}).error,ContextualDynamicRangeDecodeError::none);
    for(const auto& op:ops) {
        std::uint32_t value=999;
        const auto decoded=op.kind==ModeledOperationKind::symbol
            ? decoder.decode_symbol(op.context_id,op.alphabet_size,value)
            : decoder.decode_bypass(op.bit_count,value);
        ASSERT_EQ(decoded.error,ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(value,op.value);
    }
    EXPECT_EQ(decoder.finish(12,12).error,ContextualDynamicRangeDecodeError::none);
}

TEST(LzssReducedLiteralOperations, InvalidLateTokenAndSmallOutputDoNotWrite) {
    auto bad=tokens; bad.back()={LzssTypedTokenKind::match,0,99,3};
    std::array<ModeledOperation,12> ops{};
    for(auto& op:ops) op.value=777;
    EXPECT_NE(model_lzss_reduced_literal_tokens(bad,parameters,frame,{},ops).error,LzssFieldContextError::none);
    for(const auto& op:ops) EXPECT_EQ(op.value,777);
    EXPECT_EQ(model_lzss_reduced_literal_tokens(tokens,parameters,frame,{},std::span{ops}.first(11)).error,
        LzssFieldContextError::output_too_small);
    for(const auto& op:ops) EXPECT_EQ(op.value,777);
    auto limits=marc::core::DecoderLimits{}; limits.max_block_size=1;
    limits.max_internal_buffered_bytes=sizeof(ops)-1;
    EXPECT_EQ(model_lzss_reduced_literal_tokens(tokens,parameters,frame,limits,ops).error,LzssFieldContextError::limit_exceeded);
    for(const auto& op:ops) EXPECT_EQ(op.value,777);
}
} // namespace

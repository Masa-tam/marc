#include "context/lzss_position_distance_range_tokens.hpp"
#include "entropy/lzss_position_distance_range_decoder.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <span>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
using Error = LzssContextualRangeDecodeError;
constexpr LzssParameters parameters{65536,3,258,0};
constexpr LzssFieldContextValidationContext context{17,36,38,21,0};
constexpr ContextualDynamicRangeDescriptor descriptor{38,18,40};
constexpr std::array payload{std::byte{0},std::byte{48},std::byte{152},std::byte{79},
    std::byte{209},std::byte{96},std::byte{9},std::byte{207},std::byte{77},
    std::byte{61},std::byte{39},std::byte{231},std::byte{140},std::byte{67},
    std::byte{173},std::byte{72},std::byte{11},std::byte{64}};

TEST(LzssPositionDistanceRangeTokens, FixedVectorHistoryAndExactOutputExtent) {
    const auto valid=validate_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{});
    ASSERT_EQ(valid.error,Error::none);
    EXPECT_EQ(valid.token_count,17); EXPECT_EQ(valid.raw_size,21);
    std::array<LzssTypedToken,18> tokens{}; tokens.back().literal=0xa5;
    ASSERT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{},tokens).error,Error::none);
    for (std::size_t i=0;i<16;++i) {
        EXPECT_EQ(tokens[i].kind,LzssTypedTokenKind::literal);
        EXPECT_EQ(tokens[i].literal,97);
    }
    EXPECT_EQ(tokens[16].kind,LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[16].length,5); EXPECT_EQ(tokens[16].distance,13);
    EXPECT_EQ(tokens.back().literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, InvalidHistoryDoesNotPublishEarlierLiterals) {
    // Valid arithmetic/grammar, but first Match has distance 3 after only two Literals.
    constexpr std::array invalid{std::byte{0},std::byte{48},std::byte{152},
        std::byte{190},std::byte{146},std::byte{107},std::byte{61},std::byte{34},std::byte{142}};
    std::array<LzssTypedToken,4> tokens{};
    for (auto& token:tokens) token.literal=0xa5;
    const auto result=decode_lzss_position_distance_range_tokens(
        {14,9,40},invalid,parameters,{4,14,14,9,0},{},tokens);
    EXPECT_EQ(result.error,Error::invalid_token);
    EXPECT_EQ(result.token_index,2);
    for (const auto& token:tokens) EXPECT_EQ(token.literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, CombinedMemoryThresholdAndOutputLimits) {
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=21;
    limits.max_internal_buffered_bytes=payload.size()+17*sizeof(LzssTypedToken)
        +sizeof(LzssPositionDistanceRangeDecoder);
    std::array<LzssTypedToken,17> tokens{};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,limits,tokens).error,Error::none);
    tokens[0].literal=0xa5;
    --limits.max_internal_buffered_bytes;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,limits,tokens).error,Error::limit_exceeded);
    EXPECT_EQ(tokens[0].literal,0xa5);
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,context,{},std::span{tokens}.first(16)).error,Error::output_too_small);
    EXPECT_EQ(tokens[0].literal,0xa5);
    auto c=context; c.output_already_committed=UINT64_MAX;
    EXPECT_EQ(validate_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{}).error,Error::arithmetic_overflow);
}

TEST(LzssPositionDistanceRangeTokens, CountIdentityAndRawExtentFailuresAreAtomic) {
    std::array<LzssTypedToken,17> tokens{};
    for (auto& token:tokens) token.literal=0xa5;
    auto d=descriptor; d.context_count=24;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(d,payload,parameters,context,{},tokens).error,Error::entropy_error);
    d=descriptor; ++d.decision_count;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(d,payload,parameters,context,{},tokens).error,Error::invalid_counts);
    auto c=context; c.declared_raw_size=22;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::raw_size_mismatch);
    c=context; c.declared_raw_size=20;
    EXPECT_NE(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::none);
    c=context; c.declared_event_count=35;
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,payload,parameters,c,{},tokens).error,Error::entropy_error);
    auto bad=payload; bad.back()=std::byte{65};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,bad,parameters,context,{},tokens).error,Error::entropy_error);
    for (const auto& token:tokens) EXPECT_EQ(token.literal,0xa5);
}

TEST(LzssPositionDistanceRangeTokens, RejectsPayloadAliasingTokenStorage) {
    std::array<LzssTypedToken,17> tokens{};
    std::memcpy(tokens.data(),payload.data(),payload.size());
    const auto alias=std::span<const std::byte>{reinterpret_cast<const std::byte*>(tokens.data()),payload.size()};
    EXPECT_EQ(decode_lzss_position_distance_range_tokens(descriptor,alias,parameters,context,{},tokens).error,Error::overlapping_buffers);
    EXPECT_EQ(std::memcmp(tokens.data(),payload.data(),payload.size()),0);
}
} // namespace

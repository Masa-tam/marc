#include "context/lzss_short_match_range_tokens.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

constexpr std::array hand_payload{
    std::byte{0x00}, std::byte{0x30}, std::byte{0xbf}, std::byte{0xff},
    std::byte{0x9e}, std::byte{0x80}, std::byte{0x00}};

[[nodiscard]] marc::dictionary::internal::LzssParameters parameters() {
    return {65536, 3, 258, 0};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext vector_context() {
    return {2, 5, 5, 4, 0};
}

[[nodiscard]] constexpr
marc::entropy::internal::ContextualDynamicRangeDescriptor descriptor() {
    return {5, 7, 32};
}

} // namespace

TEST(LzssShortMatchRangeTokens, ValidatesAndDecodesHandVector) {
    const auto limits = marc::core::DecoderLimits{};
    const auto checked = validate_lzss_short_match_range_tokens(
        descriptor(), hand_payload, parameters(), vector_context(), limits);
    ASSERT_EQ(checked.error, LzssContextualRangeDecodeError::none);
    EXPECT_EQ(checked.token_count, 2U);
    EXPECT_EQ(checked.raw_size, 4U);
    EXPECT_EQ(checked.entropy.event_count, 5U);
    EXPECT_EQ(checked.entropy.decision_count, 5U);

    std::array<LzssTypedToken, 2> tokens{};
    const auto decoded = decode_lzss_short_match_range_tokens(
        descriptor(), hand_payload, parameters(), vector_context(), limits,
        tokens);
    ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 0x61U);
    EXPECT_EQ(tokens[0].distance, 0U);
    EXPECT_EQ(tokens[0].length, 0U);
    EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[1].literal, 0U);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
}

TEST(LzssShortMatchRangeTokens, RejectsCountsAndParameters) {
    const auto limits = marc::core::DecoderLimits{};
    auto context = vector_context();
    context.declared_raw_size = 3;
    EXPECT_EQ(validate_lzss_short_match_range_tokens(
                  descriptor(), hand_payload, parameters(), context, limits)
                  .error,
              LzssContextualRangeDecodeError::invalid_token);
    context = vector_context();
    --context.declared_event_count;
    EXPECT_EQ(validate_lzss_short_match_range_tokens(
                  descriptor(), hand_payload, parameters(), context, limits)
                  .error,
              LzssContextualRangeDecodeError::entropy_error);
    context = vector_context();
    ++context.declared_event_count;
    EXPECT_EQ(validate_lzss_short_match_range_tokens(
                  descriptor(), hand_payload, parameters(), context, limits)
                  .error,
              LzssContextualRangeDecodeError::invalid_counts);
    auto invalid_parameters = parameters();
    invalid_parameters.min_match_length = 5;
    EXPECT_EQ(validate_lzss_short_match_range_tokens(
                  descriptor(), hand_payload, invalid_parameters,
                  vector_context(), limits).error,
              LzssContextualRangeDecodeError::invalid_parameters);
    auto invalid_descriptor = descriptor();
    invalid_descriptor.context_count = 31;
    EXPECT_EQ(validate_lzss_short_match_range_tokens(
                  invalid_descriptor, hand_payload, parameters(),
                  vector_context(), limits).error,
              LzssContextualRangeDecodeError::entropy_error);
}

TEST(LzssShortMatchRangeTokens, KeepsOutputUntouchedOnValidationFailure) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 7, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::literal, 8, 0, 0}};
    auto bad = hand_payload;
    bad[0] = std::byte{1};
    EXPECT_EQ(decode_lzss_short_match_range_tokens(
                  descriptor(), bad, parameters(), vector_context(), limits,
                  tokens).error,
              LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(tokens[0].literal, 7U);
    EXPECT_EQ(tokens[1].literal, 8U);
    EXPECT_EQ(decode_lzss_short_match_range_tokens(
                  descriptor(), hand_payload, parameters(), vector_context(),
                  limits, std::span<LzssTypedToken>{tokens}.first(1)).error,
              LzssContextualRangeDecodeError::output_too_small);
    EXPECT_EQ(tokens[0].literal, 7U);
    EXPECT_EQ(tokens[1].literal, 8U);
}

TEST(LzssShortMatchRangeTokens, RejectsOverlappingPayloadAndTokenOutput) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> storage{};
    static_assert(sizeof(storage) >= hand_payload.size());
    std::memcpy(storage.data(), hand_payload.data(), hand_payload.size());
    const auto payload = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(storage.data()),
        hand_payload.size()};
    EXPECT_EQ(decode_lzss_short_match_range_tokens(
                  descriptor(), payload, parameters(), vector_context(),
                  limits, storage).error,
              LzssContextualRangeDecodeError::overlapping_buffers);
}

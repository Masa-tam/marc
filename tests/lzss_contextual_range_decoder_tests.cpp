#include "context/lzss_contextual_range_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenError;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualDynamicRangeDecodeError;

[[nodiscard]] constexpr auto literal_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0x20}, std::byte{0x7F},
        std::byte{0xFF}, std::byte{0xBF}, std::byte{0x00}};
}

[[nodiscard]] constexpr auto match_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0xA4}, std::byte{0x3C},
        std::byte{0x3C}, std::byte{0x38}, std::byte{0x00}};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

} // namespace

TEST(LzssContextualRangeDecoder, ValidatesAndDecodesOneLiteralAtomically) {
    constexpr auto payload = literal_payload();
    const auto validated = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, literal_context(), {});
    ASSERT_EQ(validated.error, LzssContextualRangeDecodeError::none);
    EXPECT_EQ(validated.token_count, 1U);
    EXPECT_EQ(validated.raw_size, 1U);
    EXPECT_EQ(validated.entropy.event_count, 2U);
    EXPECT_EQ(validated.entropy.decision_count, 2U);
    EXPECT_EQ(validated.entropy.payload_consumed, payload.size());

    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xCC;
    const auto decoded = decode_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, literal_context(), {}, tokens);
    ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[0].distance, 0U);
    EXPECT_EQ(tokens[0].length, 0U);
    EXPECT_EQ(tokens[1].literal, 0xCC);
}

TEST(LzssContextualRangeDecoder, DecodesExtendedVariantLiteralAtomically) {
    constexpr auto payload = literal_payload();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = 1048576;
    std::array<LzssTypedToken, 1> tokens{};
    const auto decoded = decode_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, parameters, literal_context(), {}, tokens,
        LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none);
    EXPECT_EQ(decoded.token_count, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');

    EXPECT_EQ(validate_lzss_contextual_range_tokens(
                  {2, 6, 31}, payload, parameters, literal_context(), {},
                  LzssFieldContextVariant::field_context_64k).error,
              LzssContextualRangeDecodeError::invalid_parameters);
}

TEST(LzssContextualRangeDecoder, RejectsInvalidReconstructedMatch) {
    constexpr auto payload = match_payload();
    const auto result = validate_lzss_contextual_range_tokens(
        {6, 6, 31}, payload, {}, {1, 5, 6, 10, 0}, {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_index, 0U);
    EXPECT_EQ(result.token_count, 0U);
    EXPECT_EQ(result.raw_size, 0U);
    EXPECT_EQ(result.entropy.event_count, 5U);
    EXPECT_EQ(result.entropy.decision_count, 6U);
}

TEST(LzssContextualRangeDecoder, RejectsEntropyAndCountMismatches) {
    auto malformed = literal_payload();
    malformed[0] = std::byte{1};
    auto result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, malformed, {}, literal_context(), {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(result.entropy.error,
              ContextualDynamicRangeDecodeError::invalid_interval);

    constexpr auto payload = literal_payload();
    result = validate_lzss_contextual_range_tokens(
        {3, 6, 31}, payload, {}, {1, 2, 3, 1, 0}, {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(result.entropy.error,
              ContextualDynamicRangeDecodeError::count_mismatch);

    result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, {1, 3, 2, 1, 0}, {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::invalid_counts);

    result = validate_lzss_contextual_range_tokens(
        {3, 6, 31}, payload, {}, literal_context(), {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::invalid_counts);

    result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, {1, 2, 2, 2, 0}, {});
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::raw_size_mismatch);
}

TEST(LzssContextualRangeDecoder, PrewriteFailuresPreserveAllTokens) {
    constexpr auto payload = literal_payload();
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    auto result = decode_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, literal_context(), {},
        std::span<LzssTypedToken>{tokens}.first(0));
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::output_too_small);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);

    auto malformed = payload;
    malformed[0] = std::byte{1};
    result = decode_lzss_contextual_range_tokens(
        {2, 6, 31}, malformed, {}, literal_context(), {}, tokens);
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
}

TEST(LzssContextualRangeDecoder, RejectsPayloadTokenAliasingAtomically) {
    std::array<LzssTypedToken, 2> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    constexpr auto payload = literal_payload();
    std::ranges::copy(payload, bytes.begin());
    const std::array original{
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]};

    const auto result = decode_lzss_contextual_range_tokens(
        {2, 6, 31}, std::span<const std::byte>{bytes}.first(6), {},
        literal_context(), {}, std::span<LzssTypedToken>{storage}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRangeDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        original, std::span<const std::byte>{bytes}.first(6)));
}

TEST(LzssContextualRangeDecoder, EnforcesParametersStorageAndAggregateLimits) {
    constexpr auto payload = literal_payload();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, parameters, literal_context(), {});
    EXPECT_EQ(result.error,
              LzssContextualRangeDecodeError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    limits.max_block_size = 1;
    result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, literal_context(), limits);
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_contextual_range_tokens(
        {2, 6, 31}, payload, {}, context, limits);
    EXPECT_EQ(result.error, LzssContextualRangeDecodeError::limit_exceeded);
}

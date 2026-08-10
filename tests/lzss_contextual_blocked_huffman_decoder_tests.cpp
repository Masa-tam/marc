#include "context/lzss_contextual_blocked_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenError;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualBlockedHuffmanDecodeError;
using marc::entropy::internal::ContextualBlockedHuffmanDescriptor;
using marc::entropy::internal::HuffmanDecodeTable;
using marc::entropy::internal::contextual_blocked_huffman_no_single_symbol;

[[nodiscard]] ContextualBlockedHuffmanDescriptor literal_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.field_active_mask = 0x03;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol = 0;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor literal_match_descriptor() {
    auto descriptor = literal_descriptor();
    descriptor.decision_count = 5;
    descriptor.payload_size = 1;
    descriptor.final_valid_bits = 2;
    descriptor.field_active_mask = 0x0F;
    descriptor.field_models[0].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    descriptor.field_models[0].lengths[0] = 1;
    descriptor.field_models[0].lengths[1] = 1;
    descriptor.field_models[2].active = true;
    descriptor.field_models[2].single_symbol = 0;
    descriptor.field_models[3].active = true;
    descriptor.field_models[3].single_symbol = 0;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor initial_match_descriptor() {
    auto descriptor = literal_descriptor();
    descriptor.decision_count = 3;
    descriptor.field_active_mask = 0x0F;
    descriptor.field_models[0].single_symbol = 1;
    descriptor.field_models[2].active = true;
    descriptor.field_models[2].single_symbol = 0;
    descriptor.field_models[3].active = true;
    descriptor.field_models[3].single_symbol = 0;
    return descriptor;
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext
literal_match_context() {
    return {2, 5, 5, 6, 0};
}

[[nodiscard]] constexpr LzssTypedToken sentinel_token() {
    return {LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU};
}

void expect_token_eq(
    const LzssTypedToken& actual, const LzssTypedToken& expected) {
    EXPECT_EQ(actual.kind, expected.kind);
    EXPECT_EQ(actual.literal, expected.literal);
    EXPECT_EQ(actual.distance, expected.distance);
    EXPECT_EQ(actual.length, expected.length);
}

} // namespace

TEST(LzssContextualBlockedHuffmanDecoder,
     ValidatesAndDecodesOneLiteralWithoutTables) {
    const auto validated = validate_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, {}, literal_context(), {}, {});
    ASSERT_EQ(validated.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(validated.token_count, 1U);
    EXPECT_EQ(validated.raw_size, 1U);
    EXPECT_EQ(validated.entropy.event_count, 2U);
    EXPECT_EQ(validated.entropy.decision_count, 2U);
    EXPECT_EQ(validated.entropy.bits_consumed, 0U);

    std::array<LzssTypedToken, 2> tokens{};
    tokens[1] = sentinel_token();
    const auto decoded = decode_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, {}, literal_context(), {}, {}, tokens);
    ASSERT_EQ(decoded.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    expect_token_eq(tokens[0], {LzssTypedTokenKind::literal, 'A', 0, 0});
    expect_token_eq(tokens[1], sentinel_token());
}

TEST(LzssContextualBlockedHuffmanDecoder,
     DecodesLiteralThenOverlappingMatch) {
    constexpr std::array payload{std::byte{0x02}};
    std::array<HuffmanDecodeTable, 1> tables{};
    const auto validated = validate_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(), payload, {}, literal_match_context(), {},
        tables);
    ASSERT_EQ(validated.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(validated.token_count, 2U);
    EXPECT_EQ(validated.raw_size, 6U);
    EXPECT_EQ(validated.entropy.event_count, 5U);
    EXPECT_EQ(validated.entropy.decision_count, 5U);
    EXPECT_EQ(validated.entropy.bits_consumed, 2U);

    std::array<LzssTypedToken, 2> tokens{};
    const auto decoded = decode_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(), payload, {}, literal_match_context(), {},
        tables, tokens);
    ASSERT_EQ(decoded.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    expect_token_eq(tokens[0], {LzssTypedTokenKind::literal, 'A', 0, 0});
    expect_token_eq(tokens[1], {LzssTypedTokenKind::match, 0, 1, 5});
}

TEST(LzssContextualBlockedHuffmanDecoder,
     RejectsInvalidReconstructedMatch) {
    const auto result = validate_lzss_contextual_blocked_huffman_tokens(
        initial_match_descriptor(), {}, {}, {1, 3, 3, 5, 0}, {}, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_index, 0U);
    EXPECT_EQ(result.token_count, 0U);
    EXPECT_EQ(result.raw_size, 0U);
    EXPECT_EQ(result.entropy.event_count, 3U);
    EXPECT_EQ(result.entropy.decision_count, 3U);
}

TEST(LzssContextualBlockedHuffmanDecoder,
     RejectsEntropyCountsAndRawSizeMismatch) {
    auto descriptor = literal_descriptor();
    descriptor.decision_count = 3;
    auto result = validate_lzss_contextual_blocked_huffman_tokens(
        descriptor, {}, {}, literal_context(), {}, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::invalid_counts);

    result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, {}, {1, 2, 2, 2, 0}, {}, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::raw_size_mismatch);

    auto trailing = literal_descriptor();
    trailing.payload_size = 1;
    trailing.final_valid_bits = 1;
    constexpr std::array payload{std::byte{0}};
    result = validate_lzss_contextual_blocked_huffman_tokens(
        trailing, payload, {}, literal_context(), {}, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::entropy_error);
    EXPECT_EQ(result.entropy.error,
              ContextualBlockedHuffmanDecodeError::trailing_bits);
}

TEST(LzssContextualBlockedHuffmanDecoder,
     PrewriteFailuresPreserveAllTokens) {
    constexpr std::array payload{std::byte{0x02}};
    std::array<HuffmanDecodeTable, 1> tables{};
    std::array<LzssTypedToken, 2> tokens{sentinel_token(), sentinel_token()};
    const auto before = tokens;
    auto result = decode_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(), payload, {}, literal_match_context(), {},
        tables, std::span<LzssTypedToken>{tokens}.first(1));
    EXPECT_EQ(result.error, LzssContextualBlockedHuffmanDecodeError::
                                token_output_too_small);
    expect_token_eq(tokens[0], before[0]);
    expect_token_eq(tokens[1], before[1]);

    auto trailing = literal_descriptor();
    trailing.payload_size = 1;
    trailing.final_valid_bits = 1;
    constexpr std::array zero{std::byte{0}};
    result = decode_lzss_contextual_blocked_huffman_tokens(
        trailing, zero, {}, literal_context(), {}, {},
        std::span<LzssTypedToken>{tokens}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::entropy_error);
    expect_token_eq(tokens[0], before[0]);
    expect_token_eq(tokens[1], before[1]);
}

TEST(LzssContextualBlockedHuffmanDecoder,
     RejectsShortAndPayloadAliasedTables) {
    constexpr std::array payload{std::byte{0x02}};
    std::array<HuffmanDecodeTable, 1> tables{};
    auto result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(), payload, {}, literal_match_context(), {},
        {});
    EXPECT_EQ(result.error, LzssContextualBlockedHuffmanDecodeError::
                                table_output_too_small);

    auto bytes = std::as_writable_bytes(std::span{tables});
    bytes[0] = std::byte{0x02};
    const auto original = bytes[0];
    result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(),
        std::span<const std::byte>{bytes}.first(1), {},
        literal_match_context(), {}, tables);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::overlapping_buffers);
    EXPECT_EQ(bytes[0], original);
}

TEST(LzssContextualBlockedHuffmanDecoder,
     RejectsPayloadAndTableTokenAliasingAtomically) {
    std::array<LzssTypedToken, 2> token_storage{
        sentinel_token(), sentinel_token()};
    auto token_bytes = std::as_writable_bytes(std::span{token_storage});
    token_bytes[0] = std::byte{0x02};
    const auto original_tokens = token_storage;
    std::array<HuffmanDecodeTable, 1> tables{};
    tables[0].node_count = 0xA5A5;
    auto result = decode_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(),
        std::span<const std::byte>{token_bytes}.first(1), {},
        literal_match_context(), {}, tables, token_storage);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::overlapping_buffers);
    expect_token_eq(token_storage[0], original_tokens[0]);
    expect_token_eq(token_storage[1], original_tokens[1]);
    EXPECT_EQ(tables[0].node_count, 0xA5A5);

    constexpr std::array payload{std::byte{0x02}};
    tables = {};
    tables[0].node_count = 0xA5A5;
    auto* aliased_tokens = reinterpret_cast<LzssTypedToken*>(tables.data());
    result = decode_lzss_contextual_blocked_huffman_tokens(
        literal_match_descriptor(), payload, {}, literal_match_context(), {},
        tables, std::span<LzssTypedToken>{aliased_tokens, 2});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::overlapping_buffers);
    EXPECT_EQ(tables[0].node_count, 0xA5A5);
}

TEST(LzssContextualBlockedHuffmanDecoder,
     EnforcesParametersStorageAndAggregateLimits) {
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, parameters, literal_context(), {}, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    limits.max_block_size = 1;
    result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, {}, literal_context(), limits, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_contextual_blocked_huffman_tokens(
        literal_descriptor(), {}, {}, context, limits, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::limit_exceeded);
}

#include "context/lzss_contextual_rans_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenError;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualRansDecodeError;
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] constexpr auto lower_bound_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

[[nodiscard]] ContextualRansDescriptor literal_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] ContextualRansDescriptor match_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 3;
    descriptor.payload_size = 8;
    const auto& offsets = lzss_field_context_offsets;
    descriptor.frequencies[offsets[0] + 1] = 4096;
    descriptor.frequencies[offsets[20]] = 4096;
    descriptor.frequencies[offsets[23]] = 4096;
    return descriptor;
}

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

} // namespace

TEST(LzssContextualRansDecoder, ValidatesAndDecodesOneLiteralAtomically) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = literal_descriptor();
    auto table_storage = tables();
    const auto validated = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, table_storage);
    ASSERT_EQ(validated.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(validated.token_count, 1U);
    EXPECT_EQ(validated.raw_size, 1U);
    EXPECT_EQ(validated.entropy.event_count, 2U);
    EXPECT_EQ(validated.entropy.decision_count, 2U);
    EXPECT_EQ(validated.entropy.payload_consumed, payload.size());

    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xCC;
    const auto decoded = decode_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, table_storage, tokens);
    ASSERT_EQ(decoded.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[0].distance, 0U);
    EXPECT_EQ(tokens[0].length, 0U);
    EXPECT_EQ(tokens[1].literal, 0xCC);
}

TEST(LzssContextualRansDecoder, RejectsInvalidReconstructedMatch) {
    constexpr auto payload = lower_bound_payload();
    auto table_storage = tables();
    const auto result = validate_lzss_contextual_rans_tokens(
        match_descriptor(), payload, {}, {1, 3, 3, 4, 0}, {}, table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_index, 0U);
    EXPECT_EQ(result.token_count, 0U);
    EXPECT_EQ(result.raw_size, 0U);
    EXPECT_EQ(result.entropy.event_count, 3U);
    EXPECT_EQ(result.entropy.decision_count, 3U);
}

TEST(LzssContextualRansDecoder, RejectsEntropyCountsAndRawSizeMismatch) {
    constexpr auto payload = lower_bound_payload();
    auto table_storage = tables();
    auto malformed = payload;
    malformed.fill(std::byte{0});
    auto result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), malformed, {}, literal_context(), {},
        table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::entropy_error);
    EXPECT_EQ(result.entropy.error, ContextualRansDecodeError::invalid_state);

    auto descriptor = literal_descriptor();
    descriptor.decision_count = 3;
    table_storage = tables();
    result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::invalid_counts);

    table_storage = tables();
    result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, {}, {1, 2, 2, 2, 0}, {},
        table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::raw_size_mismatch);
}

TEST(LzssContextualRansDecoder, PrewriteFailuresPreserveAllTokens) {
    constexpr auto payload = lower_bound_payload();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    auto result = decode_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, {}, literal_context(), {},
        table_storage, std::span<LzssTypedToken>{tokens}.first(0));
    EXPECT_EQ(result.error,
              LzssContextualRansDecodeError::token_output_too_small);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);

    auto malformed = payload;
    malformed.fill(std::byte{0});
    result = decode_lzss_contextual_rans_tokens(
        literal_descriptor(), malformed, {}, literal_context(), {},
        table_storage, tokens);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::entropy_error);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
}

TEST(LzssContextualRansDecoder, RejectsShortAndPayloadAliasedTables) {
    constexpr auto payload = lower_bound_payload();
    auto table_storage = tables();
    auto result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, {}, literal_context(), {},
        std::span{table_storage}.first(table_storage.size() - 1));
    EXPECT_EQ(result.error,
              LzssContextualRansDecodeError::table_output_too_small);

    table_storage = tables();
    auto bytes = std::as_writable_bytes(std::span{table_storage});
    std::ranges::copy(payload, bytes.begin());
    const auto original = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
        bytes[6], bytes[7]};
    result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), std::span<const std::byte>{bytes}.first(8), {},
        literal_context(), {}, table_storage);
    EXPECT_EQ(result.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        original, std::span<const std::byte>{bytes}.first(8)));
}

TEST(LzssContextualRansDecoder, RejectsPayloadTokenAliasingAtomically) {
    std::array<LzssTypedToken, 2> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    constexpr auto payload = lower_bound_payload();
    std::ranges::copy(payload, bytes.begin());
    const auto original = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7]};
    auto table_storage = tables();
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::ranges::fill(table_storage, marker);

    const auto result = decode_lzss_contextual_rans_tokens(
        literal_descriptor(), std::span<const std::byte>{bytes}.first(8), {},
        literal_context(), {}, table_storage,
        std::span<LzssTypedToken>{storage}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        original, std::span<const std::byte>{bytes}.first(8)));
    EXPECT_TRUE(std::ranges::all_of(table_storage, [](const auto& entry) {
        return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
            && entry.symbol == 0xa5;
    }));
}

TEST(LzssContextualRansDecoder, EnforcesParametersStorageAndAggregateLimits) {
    constexpr auto payload = lower_bound_payload();
    auto table_storage = tables();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, parameters, literal_context(), {},
        table_storage);
    EXPECT_EQ(result.error,
              LzssContextualRansDecodeError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    limits.max_block_size = 1;
    result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, {}, literal_context(), limits,
        table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_contextual_rans_tokens(
        literal_descriptor(), payload, {}, context, limits, table_storage);
    EXPECT_EQ(result.error, LzssContextualRansDecodeError::limit_exceeded);
}

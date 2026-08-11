#include "context/lzss_contextual_rans_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualRansFormatError;
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] constexpr auto literal_a_descriptor() {
    return std::array{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
}

[[nodiscard]] constexpr auto lower_bound_payload() {
    return std::array{std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                      std::byte{0x80}, std::byte{0x00}, std::byte{0x00},
                      std::byte{0x00}, std::byte{0x00}};
}

[[nodiscard]] std::vector<std::byte> descriptor_bytes(
    const ContextualRansDescriptor& model) {
    std::array<std::byte,
               marc::entropy::internal::contextual_rans_max_descriptor_size>
        storage{};
    std::size_t written{};
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  model, model.decision_count, model.payload_size, {}, storage,
                  written),
              ContextualRansFormatError::none);
    return {storage.begin(), storage.begin() + written};
}

[[nodiscard]] ContextualRansDescriptor match_model() {
    ContextualRansDescriptor model{};
    model.decision_count = 3;
    model.payload_size = 8;
    const auto& offsets = lzss_field_context_offsets;
    model.frequencies[offsets[0] + 1] = 4096;
    model.frequencies[offsets[20]] = 4096;
    model.frequencies[offsets[23]] = 4096;
    return model;
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

[[nodiscard]] std::vector<RansDecodeEntry> table_storage() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU};
}

[[nodiscard]] bool is_token_marker(const LzssTypedToken& token) {
    const auto marker = token_marker();
    return token.kind == marker.kind && token.literal == marker.literal
           && token.distance == marker.distance
           && token.length == marker.length;
}

TEST(LzssContextualRansTokenDecoder, ValidatesAndDecodesTypedToken) {
    constexpr auto descriptor = literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    const auto validated = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, tables);
    ASSERT_EQ(validated.format_error, ContextualRansFormatError::none);
    ASSERT_EQ(validated.decode.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(validated.decode.token_count, 1U);
    EXPECT_EQ(validated.decode.raw_size, 1U);
    EXPECT_EQ(validated.decode.entropy.event_count, 2U);
    EXPECT_EQ(validated.decode.entropy.decision_count, 2U);
    EXPECT_EQ(validated.decode.entropy.payload_consumed, payload.size());

    std::array<LzssTypedToken, 1> tokens{token_marker()};
    const auto decoded = decode_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, tables, tokens);
    ASSERT_EQ(decoded.format_error, ContextualRansFormatError::none);
    ASSERT_EQ(decoded.decode.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[0].distance, 0U);
    EXPECT_EQ(tokens[0].length, 0U);
}

TEST(LzssContextualRansTokenDecoder,
     SeparatesFormatFailureAndPreservesToken) {
    auto descriptor = literal_a_descriptor();
    descriptor[20] = std::byte{0x01};
    descriptor[21] = std::byte{0x00};
    descriptor[22] = std::byte{0x00};
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    std::array<LzssTypedToken, 1> tokens{token_marker()};

    const auto result = decode_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, tables, tokens);
    EXPECT_EQ(result.format_error,
              ContextualRansFormatError::noncanonical_representation);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::entropy_error);
    EXPECT_TRUE(is_token_marker(tokens[0]));
}

TEST(LzssContextualRansTokenDecoder,
     CapacityAndAliasingFailuresAreAtomic) {
    constexpr auto descriptor = literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    std::array<LzssTypedToken, 1> tokens{token_marker()};

    auto result = decode_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, tables,
        std::span<LzssTypedToken>{tokens}.first(0));
    EXPECT_EQ(result.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::token_output_too_small);
    EXPECT_TRUE(is_token_marker(tokens[0]));

    std::array<LzssTypedToken, 1> aliased{token_marker()};
    auto aliased_bytes = std::as_writable_bytes(std::span{aliased});
    std::ranges::copy(payload, aliased_bytes.begin());
    const auto before = std::array{
        aliased_bytes[0], aliased_bytes[1], aliased_bytes[2], aliased_bytes[3],
        aliased_bytes[4], aliased_bytes[5], aliased_bytes[6], aliased_bytes[7]};
    tables = table_storage();
    result = decode_lzss_contextual_rans_tokens(
        descriptor, std::span<const std::byte>{aliased_bytes}.first(8), {},
        literal_context(), {}, tables, aliased);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{aliased_bytes}.first(8)));

    tables = table_storage();
    const auto short_tables = std::span{tables}.first(tables.size() - 1);
    const auto validated = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), {}, short_tables);
    EXPECT_EQ(validated.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(validated.decode.error,
              LzssContextualRansDecodeError::table_output_too_small);
}

TEST(LzssContextualRansTokenDecoder,
     DescriptorCannotAliasPrivateTables) {
    constexpr auto descriptor = literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    auto table_bytes = std::as_writable_bytes(std::span{tables});
    std::ranges::copy(descriptor, table_bytes.begin());
    const std::vector<std::byte> before(
        table_bytes.begin(), table_bytes.begin() + descriptor.size());

    const auto result = validate_lzss_contextual_rans_tokens(
        std::span<const std::byte>{table_bytes}.first(descriptor.size()),
        payload, {}, literal_context(), {}, tables);
    EXPECT_EQ(result.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before,
        std::span<const std::byte>{table_bytes}.first(descriptor.size())));
}

TEST(LzssContextualRansTokenDecoder, RejectsInvalidReconstructedMatch) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = descriptor_bytes(match_model());
    auto tables = table_storage();
    const auto result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, {1, 3, 3, 4, 0}, {}, tables);
    EXPECT_EQ(result.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::invalid_token);
    EXPECT_EQ(result.decode.token_error,
              marc::dictionary::internal::LzssTypedTokenError::
                  invalid_distance);
    EXPECT_EQ(result.decode.token_index, 0U);
    EXPECT_EQ(result.decode.token_count, 0U);
    EXPECT_EQ(result.decode.raw_size, 0U);
}

TEST(LzssContextualRansTokenDecoder,
     RejectsEntropyCountsAndRawSizeMismatch) {
    constexpr auto payload = lower_bound_payload();
    constexpr auto descriptor = literal_a_descriptor();
    auto tables = table_storage();
    auto malformed = payload;
    malformed.fill(std::byte{});
    auto result = validate_lzss_contextual_rans_tokens(
        descriptor, malformed, {}, literal_context(), {}, tables);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::entropy_error);
    EXPECT_EQ(result.decode.entropy.error,
              marc::entropy::internal::ContextualRansDecodeError::
                  invalid_state);

    auto contradictory = descriptor;
    contradictory[0] = std::byte{0x03};
    tables = table_storage();
    result = validate_lzss_contextual_rans_tokens(
        contradictory, payload, {}, literal_context(), {}, tables);
    EXPECT_EQ(result.format_error,
              ContextualRansFormatError::contradictory_size);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::entropy_error);

    tables = table_storage();
    result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, {1, 2, 2, 2, 0}, {}, tables);
    EXPECT_EQ(result.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::raw_size_mismatch);
}

TEST(LzssContextualRansTokenDecoder,
     EnforcesParametersStorageAndAggregateLimits) {
    constexpr auto payload = lower_bound_payload();
    constexpr auto descriptor = literal_a_descriptor();
    auto tables = table_storage();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, parameters, literal_context(), {}, tables);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    limits.max_block_size = 1;
    result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, literal_context(), limits, tables);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_contextual_rans_tokens(
        descriptor, payload, {}, context, limits, tables);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::limit_exceeded);
}

} // namespace

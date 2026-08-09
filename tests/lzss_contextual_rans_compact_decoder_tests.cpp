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
using marc::entropy::internal::ContextualRansCompactFormatError;
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] constexpr auto compact_literal_a_descriptor() {
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

[[nodiscard]] ContextualRansDescriptor fixed_literal_a_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
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

TEST(LzssContextualRansCompactTokenDecoder,
     ValidatesAndMatchesFixedTypedToken) {
    constexpr auto descriptor = compact_literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto compact_tables = table_storage();
    const auto validated = validate_lzss_contextual_rans_compact_tokens(
        descriptor, payload, {}, literal_context(), {}, compact_tables);
    ASSERT_EQ(validated.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(validated.decode.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(validated.decode.token_count, 1U);
    EXPECT_EQ(validated.decode.raw_size, 1U);
    EXPECT_EQ(validated.decode.entropy.event_count, 2U);
    EXPECT_EQ(validated.decode.entropy.decision_count, 2U);
    EXPECT_EQ(validated.decode.entropy.payload_consumed, payload.size());

    std::array<LzssTypedToken, 1> compact_tokens{token_marker()};
    const auto compact = decode_lzss_contextual_rans_compact_tokens(
        descriptor, payload, {}, literal_context(), {}, compact_tables,
        compact_tokens);
    ASSERT_EQ(compact.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(compact.decode.error, LzssContextualRansDecodeError::none);

    auto fixed_tables = table_storage();
    std::array<LzssTypedToken, 1> fixed_tokens{token_marker()};
    const auto fixed = decode_lzss_contextual_rans_tokens(
        fixed_literal_a_descriptor(), payload, {}, literal_context(), {},
        fixed_tables, fixed_tokens);
    ASSERT_EQ(fixed.error, LzssContextualRansDecodeError::none);
    EXPECT_EQ(compact_tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(compact_tokens[0].literal, 'A');
    EXPECT_EQ(compact_tokens[0].distance, 0U);
    EXPECT_EQ(compact_tokens[0].length, 0U);
    EXPECT_EQ(compact_tokens[0].kind, fixed_tokens[0].kind);
    EXPECT_EQ(compact_tokens[0].literal, fixed_tokens[0].literal);
    EXPECT_EQ(compact_tokens[0].distance, fixed_tokens[0].distance);
    EXPECT_EQ(compact_tokens[0].length, fixed_tokens[0].length);
}

TEST(LzssContextualRansCompactTokenDecoder,
     SeparatesFormatFailureAndPreservesToken) {
    auto descriptor = compact_literal_a_descriptor();
    descriptor[20] = std::byte{0x01};
    descriptor[21] = std::byte{0x00};
    descriptor[22] = std::byte{0x00};
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    std::array<LzssTypedToken, 1> tokens{token_marker()};

    const auto result = decode_lzss_contextual_rans_compact_tokens(
        descriptor, payload, {}, literal_context(), {}, tables, tokens);
    EXPECT_EQ(result.format_error,
              ContextualRansCompactFormatError::noncanonical_representation);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::entropy_error);
    EXPECT_TRUE(is_token_marker(tokens[0]));
}

TEST(LzssContextualRansCompactTokenDecoder,
     CapacityAndAliasingFailuresAreAtomic) {
    constexpr auto descriptor = compact_literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    std::array<LzssTypedToken, 1> tokens{token_marker()};

    auto result = decode_lzss_contextual_rans_compact_tokens(
        descriptor, payload, {}, literal_context(), {}, tables,
        std::span<LzssTypedToken>{tokens}.first(0));
    EXPECT_EQ(result.format_error, ContextualRansCompactFormatError::none);
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
    result = decode_lzss_contextual_rans_compact_tokens(
        descriptor, std::span<const std::byte>{aliased_bytes}.first(8), {},
        literal_context(), {}, tables, aliased);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{aliased_bytes}.first(8)));

    tables = table_storage();
    const auto short_tables = std::span{tables}.first(tables.size() - 1);
    const auto validated = validate_lzss_contextual_rans_compact_tokens(
        descriptor, payload, {}, literal_context(), {}, short_tables);
    EXPECT_EQ(validated.format_error, ContextualRansCompactFormatError::none);
    EXPECT_EQ(validated.decode.error,
              LzssContextualRansDecodeError::table_output_too_small);
}

TEST(LzssContextualRansCompactTokenDecoder,
     DescriptorCannotAliasPrivateTables) {
    constexpr auto descriptor = compact_literal_a_descriptor();
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    auto table_bytes = std::as_writable_bytes(std::span{tables});
    std::ranges::copy(descriptor, table_bytes.begin());
    const std::vector<std::byte> before(
        table_bytes.begin(), table_bytes.begin() + descriptor.size());

    const auto result = validate_lzss_contextual_rans_compact_tokens(
        std::span<const std::byte>{table_bytes}.first(descriptor.size()),
        payload, {}, literal_context(), {}, tables);
    EXPECT_EQ(result.format_error, ContextualRansCompactFormatError::none);
    EXPECT_EQ(result.decode.error,
              LzssContextualRansDecodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before,
        std::span<const std::byte>{table_bytes}.first(descriptor.size())));
}

} // namespace

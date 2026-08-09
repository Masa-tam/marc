#include "context/lzss_field_context.hpp"
#include "entropy/contextual_rans_compact_format.hpp"
#include "entropy/contextual_rans_decoder.hpp"
#include "entropy/contextual_rans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using namespace marc::entropy::internal;

[[nodiscard]] ContextualRansDescriptor literal_a_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] constexpr auto lower_bound_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

[[nodiscard]] std::vector<std::byte> compact_bytes(
    const ContextualRansDescriptor& descriptor,
    const marc::core::DecoderLimits& limits = {}) {
    std::array<std::byte, contextual_rans_compact_max_descriptor_size>
        storage{};
    std::size_t written{};
    EXPECT_EQ(serialize_contextual_rans_compact_descriptor(
                  descriptor, descriptor.decision_count,
                  descriptor.payload_size, limits, storage, written),
              ContextualRansCompactFormatError::none);
    return {storage.begin(), storage.begin() + written};
}

[[nodiscard]] std::vector<RansDecodeEntry> table_storage() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] bool table_marker(const RansDecodeEntry& entry) {
    return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
        && entry.symbol == 0xa5;
}

void fill_table_markers(std::span<RansDecodeEntry> entries) {
    std::ranges::fill(entries, RansDecodeEntry{0xa5a5, 0xa5a5, 0xa5});
}

TEST(ContextualRansCompactDecoder, DecodesSpecifiedOneLiteralVector) {
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    const auto begin = decoder.begin_compact(
        descriptor, 2, 8, payload, {}, tables);
    ASSERT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(begin.decode.error, ContextualRansDecodeError::none);
    EXPECT_EQ(begin.decode.payload_consumed, 8U);

    std::uint32_t value{0xccccccccU};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 0U);
    value = 0xccccccccU;
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansCompactDecoder, BuildsIdenticalFixedTables) {
    constexpr auto payload = lower_bound_payload();
    const auto model = literal_a_descriptor();
    const auto descriptor = compact_bytes(model);
    auto ordinary_tables = table_storage();
    auto compact_tables = table_storage();
    ContextualRansDecoder ordinary;
    ContextualRansDecoder compact;
    ASSERT_EQ(ordinary.begin(model, payload, {}, ordinary_tables).error,
              ContextualRansDecodeError::none);
    const auto begin = compact.begin_compact(
        descriptor, 2, 8, payload, {}, compact_tables);
    ASSERT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(begin.decode.error, ContextualRansDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        ordinary_tables, compact_tables, [](const auto& left,
                                            const auto& right) {
            return left.cumulative == right.cumulative
                && left.frequency == right.frequency
                && left.symbol == right.symbol;
        }));
}

TEST(ContextualRansCompactDecoder, RoundTripsSymbolAndLsbFirstBypass) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 2, 2}};
    ContextualRansDescriptor model{};
    std::array<std::byte, 8> payload{};
    const auto encoded = encode_contextual_rans_operations(
        operations, {}, payload, model);
    ASSERT_EQ(encoded.error, ContextualRansEncodeError::none);
    const auto descriptor = compact_bytes(model);

    auto tables = table_storage();
    ContextualRansDecoder decoder;
    const auto begin = decoder.begin_compact(
        descriptor, model.decision_count, model.payload_size, payload, {},
        tables);
    ASSERT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(begin.decode.error, ContextualRansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 2U);
    EXPECT_EQ(decoder.finish(2, 3).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansCompactDecoder,
     DescriptorFailureIsStickyAtomicAndReusable) {
    constexpr auto payload = lower_bound_payload();
    const auto valid = compact_bytes(literal_a_descriptor());
    auto malformed = valid;
    malformed[20] = std::byte{0x01};
    malformed[21] = std::byte{0x00};
    malformed[22] = std::byte{0x00};
    auto tables = table_storage();
    fill_table_markers(tables);
    ContextualRansDecoder decoder;
    const auto failed = decoder.begin_compact(
        malformed, 2, 8, payload, {}, tables);
    EXPECT_EQ(failed.format_error,
              ContextualRansCompactFormatError::noncanonical_representation);
    EXPECT_EQ(failed.decode.error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));
    std::uint32_t value{0xccccccccU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_EQ(value, 0xccccccccU);

    const auto reused = decoder.begin_compact(
        valid, 2, 8, payload, {}, tables);
    ASSERT_EQ(reused.format_error, ContextualRansCompactFormatError::none);
    ASSERT_EQ(reused.decode.error, ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansCompactDecoder,
     SeparatesPayloadStateTableAndFormatFailures) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = compact_bytes(literal_a_descriptor());
    auto tables = table_storage();
    fill_table_markers(tables);
    ContextualRansDecoder decoder;

    auto begin = decoder.begin_compact(
        std::span<const std::byte>{descriptor}.first(descriptor.size() - 1),
        2, 8, payload, {}, tables);
    EXPECT_EQ(begin.format_error,
              ContextualRansCompactFormatError::truncated_descriptor);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    begin = decoder.begin_compact(
        descriptor, 2, 8, std::span{payload}.first<7>(), {}, tables);
    EXPECT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::payload_size_mismatch);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    auto invalid_state = payload;
    invalid_state.fill(std::byte{});
    begin = decoder.begin_compact(
        descriptor, 2, 8, invalid_state, {}, tables);
    EXPECT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    EXPECT_EQ(begin.decode.error, ContextualRansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    begin = decoder.begin_compact(
        descriptor, 2, 8, payload, {},
        std::span{tables}.first(tables.size() - 1));
    EXPECT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::table_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));
}

TEST(ContextualRansCompactDecoder, ChargesActualDescriptorExtent) {
    constexpr auto payload = lower_bound_payload();
    const auto model = literal_a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 34;
    limits.max_internal_buffered_bytes = 34;
    const auto descriptor = compact_bytes(model, limits);
    ASSERT_EQ(descriptor.size(), 26U);

    auto tables = table_storage();
    ContextualRansDecoder compact;
    const auto compact_begin = compact.begin_compact(
        descriptor, 2, 8, payload, limits, tables);
    EXPECT_EQ(compact_begin.format_error,
              ContextualRansCompactFormatError::none);
    EXPECT_EQ(compact_begin.decode.error, ContextualRansDecodeError::none);

    ContextualRansDecoder fixed;
    EXPECT_EQ(fixed.begin(model, payload, limits, tables).error,
              ContextualRansDecodeError::invalid_descriptor);
}

TEST(ContextualRansCompactDecoder, CompletedDecoderCanBeginAgain) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = compact_bytes(literal_a_descriptor());
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    for (int iteration = 0; iteration < 2; ++iteration) {
        const auto begin = decoder.begin_compact(
            descriptor, 2, 8, payload, {}, tables);
        ASSERT_EQ(begin.format_error, ContextualRansCompactFormatError::none);
        ASSERT_EQ(begin.decode.error, ContextualRansDecodeError::none);
        std::uint32_t value{};
        ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
                  ContextualRansDecodeError::none);
        ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
                  ContextualRansDecodeError::none);
        EXPECT_EQ(decoder.finish(2, 2).error,
                  ContextualRansDecodeError::none);
    }
}

} // namespace

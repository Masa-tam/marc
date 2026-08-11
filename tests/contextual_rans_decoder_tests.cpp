#include "context/lzss_field_context.hpp"
#include "entropy/contextual_rans_format.hpp"
#include "entropy/contextual_rans_decoder.hpp"
#include "entropy/contextual_rans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <span>
#include <utility>
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

[[nodiscard]] ContextualRansDescriptor descriptor_for(
    const std::uint32_t decisions,
    const std::initializer_list<std::pair<std::size_t, std::uint16_t>>
        frequencies,
    const std::uint32_t payload_size = 8) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = decisions;
    descriptor.payload_size = payload_size;
    for (const auto [index, frequency] : frequencies) {
        descriptor.frequencies[index] = frequency;
    }
    return descriptor;
}

[[nodiscard]] constexpr auto lower_bound_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

[[nodiscard]] std::vector<std::byte> descriptor_bytes(
    const ContextualRansDescriptor& descriptor,
    const marc::core::DecoderLimits& limits = {}) {
    std::array<std::byte, contextual_rans_max_descriptor_size>
        storage{};
    std::size_t written{};
    EXPECT_EQ(serialize_contextual_rans_descriptor(
                  descriptor, descriptor.decision_count,
                  descriptor.payload_size, limits, storage, written),
              ContextualRansFormatError::none);
    return {storage.begin(), storage.begin() + written};
}

[[nodiscard]] std::vector<RansDecodeEntry> table_storage() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] ContextualRansBeginResult begin_model(
    ContextualRansDecoder& decoder,
    const ContextualRansDescriptor& model,
    const std::span<const std::byte> payload,
    const std::span<RansDecodeEntry> tables) {
    const auto descriptor = descriptor_bytes(model);
    return decoder.begin(
        descriptor, model.decision_count, model.payload_size, payload, {},
        tables);
}

[[nodiscard]] bool table_marker(const RansDecodeEntry& entry) {
    return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
        && entry.symbol == 0xa5;
}

void fill_table_markers(std::span<RansDecodeEntry> entries) {
    std::ranges::fill(entries, RansDecodeEntry{0xa5a5, 0xa5a5, 0xa5});
}

TEST(ContextualRansDecoder, DecodesSpecifiedOneLiteralVector) {
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
    const auto begin = decoder.begin(
        descriptor, 2, 8, payload, {}, tables);
    ASSERT_EQ(begin.format_error, ContextualRansFormatError::none);
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

TEST(ContextualRansDecoder, BuildsCanonicalDecodeTables) {
    constexpr auto payload = lower_bound_payload();
    const auto model = literal_a_descriptor();
    const auto descriptor = descriptor_bytes(model);
    auto expected_tables = table_storage();
    auto actual_tables = table_storage();
    ContextualRansDecodeTables expected{};
    ASSERT_EQ(build_contextual_rans_decode_tables_from_model(
                  model, expected_tables, expected).error,
              ContextualRansDecodeTableError::none);
    ContextualRansDecoder decoder;
    const auto begin = decoder.begin(
        descriptor, 2, 8, payload, {}, actual_tables);
    ASSERT_EQ(begin.format_error, ContextualRansFormatError::none);
    ASSERT_EQ(begin.decode.error, ContextualRansDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        expected_tables, actual_tables, [](const auto& left,
                                           const auto& right) {
            return left.cumulative == right.cumulative
                && left.frequency == right.frequency
                && left.symbol == right.symbol;
        }));
}

TEST(ContextualRansDecoder, RoundTripsSymbolAndLsbFirstBypass) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 2, 2}};
    ContextualRansDescriptor model{};
    std::array<std::byte, 8> payload{};
    const auto encoded = encode_contextual_rans_operations(
        operations, {}, payload, model);
    ASSERT_EQ(encoded.error, ContextualRansEncodeError::none);
    const auto descriptor = descriptor_bytes(model);

    auto tables = table_storage();
    ContextualRansDecoder decoder;
    const auto begin = decoder.begin(
        descriptor, model.decision_count, model.payload_size, payload, {},
        tables);
    ASSERT_EQ(begin.format_error, ContextualRansFormatError::none);
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

TEST(ContextualRansDecoder,
     DescriptorFailureIsStickyAtomicAndReusable) {
    constexpr auto payload = lower_bound_payload();
    const auto valid = descriptor_bytes(literal_a_descriptor());
    auto malformed = valid;
    malformed[20] = std::byte{0x01};
    malformed[21] = std::byte{0x00};
    malformed[22] = std::byte{0x00};
    auto tables = table_storage();
    fill_table_markers(tables);
    ContextualRansDecoder decoder;
    const auto failed = decoder.begin(
        malformed, 2, 8, payload, {}, tables);
    EXPECT_EQ(failed.format_error,
              ContextualRansFormatError::noncanonical_representation);
    EXPECT_EQ(failed.decode.error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));
    std::uint32_t value{0xccccccccU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_EQ(value, 0xccccccccU);

    const auto reused = decoder.begin(
        valid, 2, 8, payload, {}, tables);
    ASSERT_EQ(reused.format_error, ContextualRansFormatError::none);
    ASSERT_EQ(reused.decode.error, ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansDecoder,
     SeparatesPayloadStateTableAndFormatFailures) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = descriptor_bytes(literal_a_descriptor());
    auto tables = table_storage();
    fill_table_markers(tables);
    ContextualRansDecoder decoder;

    auto begin = decoder.begin(
        std::span<const std::byte>{descriptor}.first(descriptor.size() - 1),
        2, 8, payload, {}, tables);
    EXPECT_EQ(begin.format_error,
              ContextualRansFormatError::truncated_descriptor);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    begin = decoder.begin(
        descriptor, 2, 8, std::span{payload}.first<7>(), {}, tables);
    EXPECT_EQ(begin.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::payload_size_mismatch);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    auto invalid_state = payload;
    invalid_state.fill(std::byte{});
    begin = decoder.begin(
        descriptor, 2, 8, invalid_state, {}, tables);
    EXPECT_EQ(begin.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(begin.decode.error, ContextualRansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));

    begin = decoder.begin(
        descriptor, 2, 8, payload, {},
        std::span{tables}.first(tables.size() - 1));
    EXPECT_EQ(begin.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(begin.decode.error,
              ContextualRansDecodeError::table_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(tables, table_marker));
}

TEST(ContextualRansDecoder, ChargesActualDescriptorExtent) {
    constexpr auto payload = lower_bound_payload();
    const auto model = literal_a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 34;
    limits.max_internal_buffered_bytes = 34;
    const auto descriptor = descriptor_bytes(model, limits);
    ASSERT_EQ(descriptor.size(), 26U);

    auto tables = table_storage();
    ContextualRansDecoder decoder;
    const auto begin = decoder.begin(
        descriptor, 2, 8, payload, limits, tables);
    EXPECT_EQ(begin.format_error, ContextualRansFormatError::none);
    EXPECT_EQ(begin.decode.error, ContextualRansDecodeError::none);
}

TEST(ContextualRansDecoder, CompletedDecoderCanBeginAgain) {
    constexpr auto payload = lower_bound_payload();
    const auto descriptor = descriptor_bytes(literal_a_descriptor());
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    for (int iteration = 0; iteration < 2; ++iteration) {
        const auto begin = decoder.begin(
            descriptor, 2, 8, payload, {}, tables);
        ASSERT_EQ(begin.format_error, ContextualRansFormatError::none);
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

TEST(ContextualRansDecoder, EnforcesSchemaActivityWidthsAndBudget) {
    constexpr auto payload = lower_bound_payload();
    const auto model = descriptor_for(1, {{0, 4096}});
    std::uint32_t value{0xccccccccU};

    auto tables = table_storage();
    ContextualRansDecoder context_decoder;
    ASSERT_EQ(begin_model(context_decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(context_decoder.decode_symbol(31, 2, value).error,
              ContextualRansDecodeError::invalid_context);
    EXPECT_EQ(value, 0xccccccccU);

    tables = table_storage();
    ContextualRansDecoder alphabet_decoder;
    ASSERT_EQ(begin_model(alphabet_decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(alphabet_decoder.decode_symbol(0, 256, value).error,
              ContextualRansDecodeError::invalid_alphabet);

    tables = table_storage();
    ContextualRansDecoder inactive_decoder;
    ASSERT_EQ(begin_model(inactive_decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(inactive_decoder.decode_symbol(
                  1, lzss_field_context_alphabets[1], value).error,
              ContextualRansDecodeError::inactive_context);

    tables = table_storage();
    ContextualRansDecoder bypass_decoder;
    ASSERT_EQ(begin_model(bypass_decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(0, value).error,
              ContextualRansDecodeError::invalid_bypass_width);

    tables = table_storage();
    ContextualRansDecoder budget_decoder;
    ASSERT_EQ(begin_model(budget_decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(budget_decoder.decode_bypass(2, value).error,
              ContextualRansDecodeError::decision_count_exceeded);
}

TEST(ContextualRansDecoder, DetectsMutatedTableAndTruncatedPayload) {
    constexpr auto payload = lower_bound_payload();
    auto tables = table_storage();
    ContextualRansDecoder table_decoder;
    ASSERT_EQ(begin_model(
                  table_decoder, descriptor_for(1, {{0, 4096}}), payload,
                  tables).decode.error,
              ContextualRansDecodeError::none);
    tables[0].frequency = 0;
    std::uint32_t value{0xccccccccU};
    auto decoded = table_decoder.decode_symbol(0, 2, value);
    EXPECT_EQ(decoded.error, ContextualRansDecodeError::invalid_table);
    EXPECT_EQ(decoded.event_count, 0U);
    EXPECT_EQ(value, 0xccccccccU);

    tables = table_storage();
    ContextualRansDecoder truncated_decoder;
    ASSERT_EQ(begin_model(
                  truncated_decoder,
                  descriptor_for(1, {{0, 1}, {1, 4095}}), payload, tables)
                  .decode.error,
              ContextualRansDecodeError::none);
    decoded = truncated_decoder.decode_symbol(0, 2, value);
    EXPECT_EQ(decoded.error, ContextualRansDecodeError::truncated_payload);
    EXPECT_EQ(decoded.payload_consumed, payload.size());
    EXPECT_EQ(decoded.event_count, 0U);
    EXPECT_EQ(value, 0xccccccccU);
}

TEST(ContextualRansDecoder, FinishRejectsCountsModelsStateAndTrailingData) {
    constexpr auto payload = lower_bound_payload();
    std::uint32_t value{};

    auto tables = table_storage();
    ContextualRansDecoder count_decoder;
    ASSERT_EQ(begin_model(
                  count_decoder, descriptor_for(1, {{0, 4096}}), payload,
                  tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(count_decoder.finish(2, 1).error,
              ContextualRansDecodeError::count_mismatch);

    tables = table_storage();
    ContextualRansDecoder unused_decoder;
    ASSERT_EQ(begin_model(
                  unused_decoder,
                  descriptor_for(1, {{0, 4096}, {71, 4096}}), payload,
                  tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(unused_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(unused_decoder.finish(1, 1).error,
              ContextualRansDecodeError::unused_context);

    constexpr std::array invalid_state_payload{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    tables = table_storage();
    ContextualRansDecoder state_decoder;
    ASSERT_EQ(begin_model(
                  state_decoder, descriptor_for(1, {{0, 4096}}),
                  invalid_state_payload, tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(state_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(state_decoder.finish(1, 1).error,
              ContextualRansDecodeError::invalid_terminal_state);

    constexpr std::array trailing_payload{
        payload[0], payload[1], payload[2], payload[3], payload[4],
        payload[5], payload[6], payload[7], std::byte{0}};
    tables = table_storage();
    ContextualRansDecoder trailing_decoder;
    ASSERT_EQ(begin_model(
                  trailing_decoder, descriptor_for(1, {{0, 4096}}, 9),
                  trailing_payload, tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(1, 1).error,
              ContextualRansDecodeError::trailing_payload);
}

TEST(ContextualRansDecoder, RequiresBeginAndEndsConsistently) {
    ContextualRansDecoder decoder;
    std::uint32_t value{0xccccccccU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::not_started);

    constexpr auto payload = lower_bound_payload();
    const auto model = descriptor_for(1, {{0, 4096}});
    auto tables = table_storage();
    ASSERT_EQ(begin_model(decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.finish(1, 1).error, ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(1, 1).error,
              ContextualRansDecodeError::already_finished);

    tables = table_storage();
    ASSERT_EQ(begin_model(decoder, model, payload, tables).decode.error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(1, 1).error, ContextualRansDecodeError::none);
}

} // namespace

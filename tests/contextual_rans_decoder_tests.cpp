#include "entropy/contextual_rans_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace {

using namespace marc::entropy::internal;

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

[[nodiscard]] std::vector<RansDecodeEntry> table_storage() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

} // namespace

TEST(ContextualRansDecoder, DecodesDocumentedOneLiteralVector) {
    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(2, {{0, 4096}, {71, 4096}});
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    auto result = decoder.begin(descriptor, payload, {}, tables);
    ASSERT_EQ(result.error, ContextualRansDecodeError::none);
    EXPECT_EQ(result.payload_consumed, 8U);

    std::uint32_t value{0xCCCCCCCCU};
    result = decoder.decode_symbol(0, 2, value);
    ASSERT_EQ(result.error, ContextualRansDecodeError::none);
    EXPECT_EQ(value, 0U);
    value = 0xCCCCCCCCU;
    result = decoder.decode_symbol(3, 256, value);
    ASSERT_EQ(result.error, ContextualRansDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansDecoder, DecodesLsbFirstFixedBypassVector) {
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    auto descriptor = descriptor_for(3, {{1, 4096}});
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 2U);
    EXPECT_EQ(decoder.finish(2, 3).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansDecoder, RejectsBeginFailuresBeforePublishingState) {
    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(1, {{0, 4096}});
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::vector<RansDecodeEntry> tables(
        contextual_rans_decode_table_entries, marker);
    ContextualRansDecoder decoder;

    auto invalid = descriptor;
    invalid.frequencies[0] = 4095;
    EXPECT_EQ(decoder.begin(invalid, payload, {}, tables).error,
              ContextualRansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto& entry) {
        return entry.frequency == 0xa5a5;
    }));

    EXPECT_EQ(decoder.begin(
                  descriptor, std::span{payload}.first<7>(), {}, tables).error,
              ContextualRansDecodeError::payload_size_mismatch);
    EXPECT_EQ(decoder.begin(
                  descriptor, payload, {},
                  std::span{tables}.first(tables.size() - 1)).error,
              ContextualRansDecodeError::table_output_too_small);

    auto invalid_state = payload;
    invalid_state.fill(std::byte{0});
    EXPECT_EQ(decoder.begin(descriptor, invalid_state, {}, tables).error,
              ContextualRansDecodeError::invalid_state);
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::invalid_state);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualRansDecoder, EnforcesSchemaActivityWidthsAndBudget) {
    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(1, {{0, 4096}});
    std::uint32_t value{0xCCCCCCCCU};

    auto tables = table_storage();
    ContextualRansDecoder context_decoder;
    ASSERT_EQ(context_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(context_decoder.decode_symbol(31, 2, value).error,
              ContextualRansDecodeError::invalid_context);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    tables = table_storage();
    ContextualRansDecoder alphabet_decoder;
    ASSERT_EQ(alphabet_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(alphabet_decoder.decode_symbol(0, 256, value).error,
              ContextualRansDecodeError::invalid_alphabet);

    tables = table_storage();
    ContextualRansDecoder inactive_decoder;
    ASSERT_EQ(inactive_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(inactive_decoder.decode_symbol(
                  1,
                  marc::context::internal::lzss_field_context_alphabets[1],
                  value).error,
              ContextualRansDecodeError::inactive_context);

    tables = table_storage();
    ContextualRansDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(0, value).error,
              ContextualRansDecodeError::invalid_bypass_width);

    tables = table_storage();
    ContextualRansDecoder budget_decoder;
    ASSERT_EQ(budget_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(budget_decoder.decode_bypass(2, value).error,
              ContextualRansDecodeError::decision_count_exceeded);
}

TEST(ContextualRansDecoder, DetectsMutatedTableAndPreservesValue) {
    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(1, {{0, 4096}});
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    tables[0].frequency = 0;
    std::uint32_t value{0xCCCCCCCCU};
    const auto result = decoder.decode_symbol(0, 2, value);
    EXPECT_EQ(result.error, ContextualRansDecodeError::invalid_table);
    EXPECT_EQ(result.event_count, 0U);
    EXPECT_EQ(result.decision_count, 0U);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualRansDecoder, DetectsTruncatedRenormalization) {
    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(1, {{0, 1}, {1, 4095}});
    auto tables = table_storage();
    ContextualRansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    std::uint32_t value{0xCCCCCCCCU};
    const auto result = decoder.decode_symbol(0, 2, value);
    EXPECT_EQ(result.error, ContextualRansDecodeError::truncated_payload);
    EXPECT_EQ(result.payload_consumed, payload.size());
    EXPECT_EQ(result.event_count, 0U);
    EXPECT_EQ(result.decision_count, 0U);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualRansDecoder, FinishRejectsCountsUnusedModelsStateAndTrailing) {
    constexpr auto payload = lower_bound_payload();
    std::uint32_t value{};

    auto tables = table_storage();
    ContextualRansDecoder count_decoder;
    auto descriptor = descriptor_for(1, {{0, 4096}});
    ASSERT_EQ(count_decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(count_decoder.finish(2, 1).error,
              ContextualRansDecodeError::count_mismatch);

    tables = table_storage();
    ContextualRansDecoder unused_decoder;
    descriptor = descriptor_for(1, {{0, 4096}, {71, 4096}});
    ASSERT_EQ(unused_decoder.begin(descriptor, payload, {}, tables).error,
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
    descriptor = descriptor_for(1, {{0, 4096}});
    ASSERT_EQ(state_decoder.begin(
                  descriptor, invalid_state_payload, {}, tables).error,
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
    descriptor = descriptor_for(1, {{0, 4096}}, 9);
    ASSERT_EQ(trailing_decoder.begin(
                  descriptor, trailing_payload, {}, tables).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(1, 1).error,
              ContextualRansDecodeError::trailing_payload);
}

TEST(ContextualRansDecoder, RequiresBeginAndEndsConsistently) {
    ContextualRansDecoder decoder;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::not_started);

    constexpr auto payload = lower_bound_payload();
    auto descriptor = descriptor_for(1, {{0, 4096}});
    auto tables = table_storage();
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.finish(1, 1).error, ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(1, 1).error,
              ContextualRansDecodeError::already_finished);

    tables = table_storage();
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualRansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(decoder.finish(1, 1).error, ContextualRansDecodeError::none);
}

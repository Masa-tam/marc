#include "entropy/contextual_tans_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using namespace marc::entropy::internal;

[[nodiscard]] ContextualTansDescriptor descriptor_for_literal_a(
    const std::uint32_t payload_size = 2,
    const std::uint8_t final_valid_bits = 0) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = payload_size;
    descriptor.final_valid_bits = final_valid_bits;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] std::vector<TansDecodeEntry> table_storage() {
    return std::vector<TansDecodeEntry>(contextual_tans_decode_table_entries);
}

struct EncodedStep {
    std::uint32_t value{};
    std::uint8_t bit_count{};
};

[[nodiscard]] bool reverse_step(
    const TansDescriptor& descriptor,
    const TansTables& tables,
    const std::uint8_t symbol,
    std::uint32_t& state,
    EncodedStep& step) {
    const auto frequency = descriptor.frequencies[symbol];
    bool found{};
    std::uint32_t next{};
    for (std::uint8_t bits = 0; bits <= tans_table_log; ++bits) {
        const auto reduced = state >> bits;
        if (reduced < frequency || reduced >= 2U * frequency) continue;
        const auto index = static_cast<std::uint32_t>(
            tables.symbol_offsets[symbol]) + reduced - frequency;
        if (index >= tables.encode_states.size()) return false;
        const auto candidate = tables.encode_states[index];
        if (candidate < tans_table_size || candidate >= 2U * tans_table_size
            || tables.decode[candidate - tans_table_size].bit_count != bits
            || found) {
            return false;
        }
        found = true;
        next = candidate;
        step.bit_count = bits;
        step.value = state & ((UINT32_C(1) << bits) - 1U);
    }
    if (!found) return false;
    state = next;
    return true;
}

[[nodiscard]] std::vector<std::byte> encode_symbol_then_bypass(
    ContextualTansDescriptor& descriptor) {
    TansDescriptor symbol_descriptor{};
    symbol_descriptor.frequencies[1] = 4096;
    TansDescriptor bypass_descriptor{};
    bypass_descriptor.frequencies[0] = 2048;
    bypass_descriptor.frequencies[1] = 2048;
    TansTables symbol_tables{};
    TansTables bypass_tables{};
    EXPECT_EQ(build_tans_tables(symbol_descriptor, symbol_tables),
              TansTableError::none);
    EXPECT_EQ(build_tans_tables(bypass_descriptor, bypass_tables),
              TansTableError::none);

    // Forward decisions are Symbol(1), then bypass bits 0 and 1. Encoding
    // traverses both operations and the bits inside bypass in reverse.
    std::array<EncodedStep, 3> reverse_steps{};
    std::uint32_t state = tans_table_size;
    EXPECT_TRUE(reverse_step(
        bypass_descriptor, bypass_tables, 1, state, reverse_steps[0]));
    EXPECT_TRUE(reverse_step(
        bypass_descriptor, bypass_tables, 0, state, reverse_steps[1]));
    EXPECT_TRUE(reverse_step(
        symbol_descriptor, symbol_tables, 1, state, reverse_steps[2]));

    std::size_t total_bits{};
    for (const auto& step : reverse_steps) total_bits += step.bit_count;
    std::vector<std::byte> payload(2 + (total_bits + 7) / 8);
    payload[0] = static_cast<std::byte>(state - tans_table_size);
    payload[1] = static_cast<std::byte>((state - tans_table_size) >> 8);
    std::size_t position{};
    for (auto index = reverse_steps.size(); index != 0; --index) {
        const auto& step = reverse_steps[index - 1];
        for (std::uint8_t bit = 0; bit < step.bit_count; ++bit) {
            if (((step.value >> bit) & 1U) != 0) {
                payload[2 + position / 8] |=
                    static_cast<std::byte>(1U << (position % 8));
            }
            ++position;
        }
    }

    descriptor = {};
    descriptor.decision_count = 3;
    descriptor.payload_size = static_cast<std::uint32_t>(payload.size());
    descriptor.final_valid_bits = total_bits == 0 ? 0
        : static_cast<std::uint8_t>((total_bits - 1) % 8 + 1);
    descriptor.frequencies[1] = 4096;
    return payload;
}

TEST(ContextualTansDecoder, DecodesDocumentedOneLiteralVector) {
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    auto tables = table_storage();
    ContextualTansDecoder decoder;
    auto result = decoder.begin(
        descriptor_for_literal_a(), payload, {}, tables);
    ASSERT_EQ(result.error, ContextualTansDecodeError::none);

    std::uint32_t value{0xCCCCCCCCU};
    result = decoder.decode_symbol(0, 2, value);
    ASSERT_EQ(result.error, ContextualTansDecodeError::none);
    EXPECT_EQ(value, 0U);
    result = decoder.decode_symbol(3, 256, value);
    ASSERT_EQ(result.error, ContextualTansDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualTansDecodeError::none);
}

TEST(ContextualTansDecoder, DecodesMixedSymbolAndLsbFirstBypass) {
    ContextualTansDescriptor descriptor{};
    const auto payload = encode_symbol_then_bypass(descriptor);
    auto tables = table_storage();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualTansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 2U);
    EXPECT_EQ(decoder.finish(2, 3).error, ContextualTansDecodeError::none);
}

TEST(ContextualTansDecoder, RejectsBeginFailuresBeforePublishingState) {
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    const TansDecodeEntry marker{0xa5a5, 0xa5, 0xa5};
    std::vector<TansDecodeEntry> tables(
        contextual_tans_decode_table_entries, marker);
    ContextualTansDecoder decoder;

    auto invalid = descriptor_for_literal_a();
    invalid.frequencies[0] = 4095;
    EXPECT_EQ(decoder.begin(invalid, payload, {}, tables).error,
              ContextualTansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto& entry) {
        return entry.state_base == 0xa5a5;
    }));
    EXPECT_EQ(decoder.begin(
                  descriptor_for_literal_a(),
                  std::span{payload}.first<1>(), {}, tables).error,
              ContextualTansDecodeError::payload_size_mismatch);
    EXPECT_EQ(decoder.begin(
                  descriptor_for_literal_a(), payload, {},
                  std::span{tables}.first(tables.size() - 1)).error,
              ContextualTansDecodeError::table_output_too_small);

    constexpr std::array invalid_state{
        std::byte{0x00}, std::byte{0x10}};
    EXPECT_EQ(decoder.begin(
                  descriptor_for_literal_a(), invalid_state, {}, tables).error,
              ContextualTansDecodeError::invalid_state);
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::invalid_state);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualTansDecoder, RejectsPaddingAndTableLimitBeforeWriting) {
    auto descriptor = descriptor_for_literal_a(3, 1);
    constexpr std::array padded{
        std::byte{0}, std::byte{0}, std::byte{0x80}};
    const TansDecodeEntry marker{0xa5a5, 0xa5, 0xa5};
    std::vector<TansDecodeEntry> tables(
        contextual_tans_decode_table_entries, marker);
    ContextualTansDecoder decoder;
    EXPECT_EQ(decoder.begin(descriptor, padded, {}, tables).error,
              ContextualTansDecodeError::nonzero_padding);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto& entry) {
        return entry.state_base == 0xa5a5;
    }));

    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = contextual_tans_decode_table_entries - 1;
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    EXPECT_EQ(decoder.begin(
                  descriptor_for_literal_a(), payload, limits, tables).error,
              ContextualTansDecodeError::invalid_descriptor);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto& entry) {
        return entry.state_base == 0xa5a5;
    }));
}

TEST(ContextualTansDecoder, EnforcesSchemaActivityWidthsAndBudget) {
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    std::uint32_t value{0xCCCCCCCCU};

    auto tables = table_storage();
    ContextualTansDecoder context_decoder;
    ASSERT_EQ(context_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(context_decoder.decode_symbol(31, 2, value).error,
              ContextualTansDecodeError::invalid_context);

    tables = table_storage();
    ContextualTansDecoder alphabet_decoder;
    ASSERT_EQ(alphabet_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(alphabet_decoder.decode_symbol(0, 256, value).error,
              ContextualTansDecodeError::invalid_alphabet);

    tables = table_storage();
    ContextualTansDecoder inactive_decoder;
    ASSERT_EQ(inactive_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(inactive_decoder.decode_symbol(1, 2, value).error,
              ContextualTansDecodeError::inactive_context);

    tables = table_storage();
    ContextualTansDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(0, value).error,
              ContextualTansDecodeError::invalid_bypass_width);

    tables = table_storage();
    ContextualTansDecoder budget_decoder;
    ASSERT_EQ(budget_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(budget_decoder.decode_bypass(3, value).error,
              ContextualTansDecodeError::decision_count_exceeded);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualTansDecoder, DetectsMutatedTableAndTruncatedBits) {
    auto descriptor = descriptor_for_literal_a(3, 1);
    constexpr std::array payload{
        std::byte{0}, std::byte{0}, std::byte{0}};
    auto tables = table_storage();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, tables).error,
              ContextualTansDecodeError::none);
    tables[0].bit_count = 2;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::truncated_bits);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    tables = table_storage();
    ContextualTansDecoder invalid_table_decoder;
    descriptor = descriptor_for_literal_a();
    ASSERT_EQ(invalid_table_decoder.begin(
                  descriptor, std::span{payload}.first<2>(), {}, tables).error,
              ContextualTansDecodeError::none);
    tables[0].state_base = 8191;
    tables[0].bit_count = 1;
    EXPECT_EQ(invalid_table_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::invalid_table);
}

TEST(ContextualTansDecoder, FinishRejectsCountsUnusedStateAndTrailingBits) {
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    std::uint32_t value{};

    auto tables = table_storage();
    ContextualTansDecoder count_decoder;
    ASSERT_EQ(count_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(count_decoder.finish(1, 1).error,
              ContextualTansDecodeError::count_mismatch);

    tables = table_storage();
    ContextualTansDecoder unused_decoder;
    ASSERT_EQ(unused_decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(unused_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(unused_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(unused_decoder.finish(2, 2).error,
              ContextualTansDecodeError::unused_context);

    constexpr std::array offset_one{std::byte{1}, std::byte{0}};
    tables = table_storage();
    ContextualTansDecoder state_decoder;
    auto one_context = descriptor_for_literal_a();
    one_context.frequencies[71] = 0;
    one_context.decision_count = 1;
    ASSERT_EQ(state_decoder.begin(one_context, offset_one, {}, tables).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(state_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(state_decoder.finish(1, 1).error,
              ContextualTansDecodeError::invalid_terminal_state);

    constexpr std::array trailing_payload{
        std::byte{0}, std::byte{0}, std::byte{0}};
    tables = table_storage();
    ContextualTansDecoder trailing_decoder;
    auto trailing_descriptor = descriptor_for_literal_a(3, 1);
    ASSERT_EQ(trailing_decoder.begin(
                  trailing_descriptor, trailing_payload, {}, tables).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(3, 256, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(2, 2).error,
              ContextualTansDecodeError::trailing_bits);
}

TEST(ContextualTansDecoder, RequiresBeginAndEndsConsistently) {
    ContextualTansDecoder decoder;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::not_started);
    constexpr std::array payload{std::byte{0}, std::byte{0}};
    auto tables = table_storage();
    ASSERT_EQ(decoder.begin(
                  descriptor_for_literal_a(), payload, {}, tables).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualTansDecodeError::none);
    ASSERT_EQ(decoder.finish(2, 2).error, ContextualTansDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualTansDecodeError::already_finished);
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::already_finished);
}

} // namespace

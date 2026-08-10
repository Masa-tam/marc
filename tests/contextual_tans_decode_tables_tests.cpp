#include "entropy/contextual_tans_decode_tables.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using marc::entropy::internal::ContextualTansDecodeTableError;
using marc::entropy::internal::ContextualTansDecodeTables;
using marc::entropy::internal::ContextualTansDescriptor;
using marc::entropy::internal::ContextualTansFormatError;
using marc::entropy::internal::TansDecodeEntry;
using marc::entropy::internal::TansDescriptor;
using marc::entropy::internal::TansTableError;
using marc::entropy::internal::TansTables;
using marc::entropy::internal::contextual_tans_bypass_table_index;
using marc::entropy::internal::contextual_tans_decode_table_entries;
using marc::entropy::internal::contextual_tans_total_frequency;

[[nodiscard]] ContextualTansDescriptor literal_a_descriptor() {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 2;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] bool marker(const TansDecodeEntry& entry) {
    return entry.state_base == 0xa5a5 && entry.symbol == 0xa5
        && entry.bit_count == 0xa5;
}

TEST(ContextualTansDecodeTables, BuildsFixedContextsAndBypassTable) {
    std::vector<TansDecodeEntry> storage(
        contextual_tans_decode_table_entries);
    ContextualTansDecodeTables tables{};
    const auto result =
        marc::entropy::internal::build_contextual_tans_decode_tables(
            literal_a_descriptor(), {}, storage, tables);
    ASSERT_EQ(result.error, ContextualTansDecodeTableError::none);
    EXPECT_EQ(result.required_entries, contextual_tans_decode_table_entries);
    EXPECT_EQ(result.active_context_count, 2U);
    ASSERT_EQ(tables.entries.data(), storage.data());
    ASSERT_EQ(tables.entries.size(), contextual_tans_decode_table_entries);
    EXPECT_TRUE(tables.active_contexts[0]);
    EXPECT_TRUE(tables.active_contexts[3]);
    EXPECT_FALSE(tables.active_contexts[1]);

    const auto context_zero = tables.entries.first(
        contextual_tans_total_frequency);
    EXPECT_TRUE(std::ranges::all_of(context_zero, [](const auto& entry) {
        return entry.symbol == 0 && entry.state_base >= 4096
            && entry.state_base < 8192 && entry.bit_count == 0;
    }));
    auto sorted_states = std::vector<std::uint16_t>{};
    sorted_states.reserve(context_zero.size());
    for (const auto& entry : context_zero) {
        sorted_states.push_back(entry.state_base);
    }
    std::ranges::sort(sorted_states);
    for (std::size_t index = 0; index < sorted_states.size(); ++index) {
        EXPECT_EQ(sorted_states[index], 4096U + index);
    }
    const auto literal_context = tables.entries.subspan(
        3 * contextual_tans_total_frequency,
        contextual_tans_total_frequency);
    EXPECT_TRUE(std::ranges::all_of(literal_context, [](const auto& entry) {
        return entry.symbol == 65 && entry.state_base >= 4096
            && entry.state_base < 8192 && entry.bit_count == 0;
    }));
    const auto inactive = tables.entries.subspan(
        contextual_tans_total_frequency, contextual_tans_total_frequency);
    EXPECT_TRUE(std::ranges::all_of(inactive, [](const auto& entry) {
        return entry == TansDecodeEntry{};
    }));

    TansDescriptor expected_descriptor{};
    expected_descriptor.frequencies[0] = 2048;
    expected_descriptor.frequencies[1] = 2048;
    TansTables expected{};
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(
                  expected_descriptor, expected),
              TansTableError::none);
    const auto bypass = tables.entries.subspan(
        contextual_tans_bypass_table_index
            * contextual_tans_total_frequency,
        contextual_tans_total_frequency);
    EXPECT_TRUE(std::equal(
        bypass.begin(), bypass.end(), expected.decode.begin()));
}

TEST(ContextualTansDecodeTables, ReusesCanonicalStandaloneTransitions) {
    auto descriptor = literal_a_descriptor();
    descriptor.frequencies[0] = 2731;
    descriptor.frequencies[1] = 1365;
    std::vector<TansDecodeEntry> storage(
        contextual_tans_decode_table_entries);
    ContextualTansDecodeTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_contextual_tans_decode_tables(
                  descriptor, {}, storage, tables).error,
              ContextualTansDecodeTableError::none);

    TansDescriptor expected_descriptor{};
    expected_descriptor.frequencies[0] = 2731;
    expected_descriptor.frequencies[1] = 1365;
    TansTables expected{};
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(
                  expected_descriptor, expected),
              TansTableError::none);
    EXPECT_TRUE(std::equal(
        tables.entries.begin(),
        tables.entries.begin() + contextual_tans_total_frequency,
        expected.decode.begin()));
}

TEST(ContextualTansDecodeTables, PrewriteFailuresPreserveStorageAndView) {
    const TansDecodeEntry sentinel{0xa5a5, 0xa5, 0xa5};
    std::vector<TansDecodeEntry> storage(
        contextual_tans_decode_table_entries, sentinel);
    ContextualTansDecodeTables tables{};
    tables.entries = std::span<TansDecodeEntry>{storage}.first(1);
    tables.active_contexts[7] = true;
    const auto original_view = tables.entries;

    auto invalid = literal_a_descriptor();
    invalid.frequencies[0] = 4095;
    auto result =
        marc::entropy::internal::build_contextual_tans_decode_tables(
            invalid, {}, storage, tables);
    EXPECT_EQ(result.error,
              ContextualTansDecodeTableError::invalid_descriptor);
    EXPECT_EQ(result.format_error,
              ContextualTansFormatError::invalid_frequency_table);
    EXPECT_EQ(tables.entries.data(), original_view.data());
    EXPECT_EQ(tables.entries.size(), original_view.size());
    EXPECT_TRUE(tables.active_contexts[7]);
    EXPECT_TRUE(std::ranges::all_of(storage, marker));

    result = marc::entropy::internal::build_contextual_tans_decode_tables(
        literal_a_descriptor(), {},
        std::span<TansDecodeEntry>{storage}.first(storage.size() - 1),
        tables);
    EXPECT_EQ(result.error, ContextualTansDecodeTableError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(storage, marker));

    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries =
        contextual_tans_decode_table_entries - 1;
    result = marc::entropy::internal::build_contextual_tans_decode_tables(
        literal_a_descriptor(), limits, storage, tables);
    EXPECT_EQ(result.error,
              ContextualTansDecodeTableError::invalid_descriptor);
    EXPECT_EQ(result.format_error, ContextualTansFormatError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(storage, marker));
}

TEST(ContextualTansDecodeTables, WritesOnlyRequiredEntryExtent) {
    const TansDecodeEntry sentinel{0xa5a5, 0xa5, 0xa5};
    std::vector<TansDecodeEntry> storage(
        contextual_tans_decode_table_entries + 3, sentinel);
    ContextualTansDecodeTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_contextual_tans_decode_tables(
                  literal_a_descriptor(), {}, storage, tables).error,
              ContextualTansDecodeTableError::none);
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const TansDecodeEntry>{storage}.last(3), marker));
}

} // namespace

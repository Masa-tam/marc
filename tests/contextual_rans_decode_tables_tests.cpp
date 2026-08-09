#include "entropy/contextual_rans_decode_tables.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

using marc::entropy::internal::ContextualRansDecodeTableError;
using marc::entropy::internal::ContextualRansDecodeTables;
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::ContextualRansFormatError;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;
using marc::entropy::internal::contextual_rans_total_frequency;

[[nodiscard]] ContextualRansDescriptor literal_a_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] bool sentinel(const RansDecodeEntry& entry) {
    return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
        && entry.symbol == 0xa5;
}

TEST(ContextualRansDecodeTables, BuildsFixedContextViewsForOneLiteral) {
    std::vector<RansDecodeEntry> storage(contextual_rans_decode_table_entries);
    ContextualRansDecodeTables tables{};
    const auto result =
        marc::entropy::internal::build_contextual_rans_decode_tables(
            literal_a_descriptor(), {}, storage, tables);
    ASSERT_EQ(result.error, ContextualRansDecodeTableError::none);
    EXPECT_EQ(result.required_entries, contextual_rans_decode_table_entries);
    EXPECT_EQ(result.active_context_count, 2U);
    ASSERT_EQ(tables.entries.data(), storage.data());
    ASSERT_EQ(tables.entries.size(), contextual_rans_decode_table_entries);
    EXPECT_TRUE(tables.active_contexts[0]);
    EXPECT_TRUE(tables.active_contexts[3]);
    EXPECT_FALSE(tables.active_contexts[1]);

    const auto context_zero = tables.entries.subspan(
        0, contextual_rans_total_frequency);
    EXPECT_EQ(context_zero.front().symbol, 0U);
    EXPECT_EQ(context_zero.front().frequency, 4096U);
    EXPECT_EQ(context_zero.back().symbol, 0U);
    EXPECT_EQ(context_zero.back().frequency, 4096U);

    const auto context_three = tables.entries.subspan(
        3 * contextual_rans_total_frequency,
        contextual_rans_total_frequency);
    EXPECT_EQ(context_three.front().symbol, 65U);
    EXPECT_EQ(context_three.back().symbol, 65U);
    EXPECT_EQ(context_three.back().frequency, 4096U);

    const auto context_one = tables.entries.subspan(
        contextual_rans_total_frequency, contextual_rans_total_frequency);
    EXPECT_TRUE(std::ranges::all_of(context_one, [](const auto& entry) {
        return entry.frequency == 0;
    }));
}

TEST(ContextualRansDecodeTables, MapsCanonicalMultiSymbolRanges) {
    auto descriptor = literal_a_descriptor();
    descriptor.frequencies[0] = 2731;
    descriptor.frequencies[1] = 1365;
    std::vector<RansDecodeEntry> storage(contextual_rans_decode_table_entries);
    ContextualRansDecodeTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_contextual_rans_decode_tables(
                  descriptor, {}, storage, tables).error,
              ContextualRansDecodeTableError::none);
    const auto context_zero = tables.entries.first(
        contextual_rans_total_frequency);
    EXPECT_EQ(context_zero[0].symbol, 0U);
    EXPECT_EQ(context_zero[2730].symbol, 0U);
    EXPECT_EQ(context_zero[2731].symbol, 1U);
    EXPECT_EQ(context_zero[4095].symbol, 1U);
    EXPECT_EQ(context_zero[2731].cumulative, 2731U);
    EXPECT_EQ(context_zero[2731].frequency, 1365U);
}

TEST(ContextualRansDecodeTables, PrewriteFailuresPreserveStorageAndView) {
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::vector<RansDecodeEntry> storage(
        contextual_rans_decode_table_entries, marker);
    ContextualRansDecodeTables tables{};
    tables.entries = std::span<RansDecodeEntry>{storage}.first(1);
    tables.active_contexts[7] = true;
    const auto original_view = tables.entries;

    auto invalid = literal_a_descriptor();
    invalid.frequencies[0] = 4095;
    auto result = marc::entropy::internal::build_contextual_rans_decode_tables(
        invalid, {}, storage, tables);
    EXPECT_EQ(result.error,
              ContextualRansDecodeTableError::invalid_descriptor);
    EXPECT_EQ(result.format_error,
              ContextualRansFormatError::invalid_frequency_table);
    EXPECT_EQ(tables.entries.data(), original_view.data());
    EXPECT_EQ(tables.entries.size(), original_view.size());
    EXPECT_TRUE(tables.active_contexts[7]);
    EXPECT_TRUE(std::ranges::all_of(storage, sentinel));

    result = marc::entropy::internal::
        build_contextual_rans_decode_tables_from_model(
            invalid, storage, tables);
    EXPECT_EQ(result.error,
              ContextualRansDecodeTableError::invalid_descriptor);
    EXPECT_EQ(result.format_error,
              ContextualRansFormatError::invalid_frequency_table);
    EXPECT_EQ(tables.entries.data(), original_view.data());
    EXPECT_EQ(tables.entries.size(), original_view.size());
    EXPECT_TRUE(tables.active_contexts[7]);
    EXPECT_TRUE(std::ranges::all_of(storage, sentinel));

    result = marc::entropy::internal::build_contextual_rans_decode_tables(
        literal_a_descriptor(), {},
        std::span<RansDecodeEntry>{storage}.first(storage.size() - 1), tables);
    EXPECT_EQ(result.error, ContextualRansDecodeTableError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(storage, sentinel));

    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = contextual_rans_decode_table_entries - 1;
    result = marc::entropy::internal::build_contextual_rans_decode_tables(
        literal_a_descriptor(), limits, storage, tables);
    EXPECT_EQ(result.error,
              ContextualRansDecodeTableError::invalid_descriptor);
    EXPECT_EQ(result.format_error, ContextualRansFormatError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(storage, sentinel));
}

TEST(ContextualRansDecodeTables, WritesOnlyRequiredEntryExtent) {
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::vector<RansDecodeEntry> storage(
        contextual_rans_decode_table_entries + 3, marker);
    ContextualRansDecodeTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_contextual_rans_decode_tables(
                  literal_a_descriptor(), {}, storage, tables).error,
              ContextualRansDecodeTableError::none);
    EXPECT_EQ(tables.entries.size(), contextual_rans_decode_table_entries);
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const RansDecodeEntry>{storage}.last(3), sentinel));
}

} // namespace

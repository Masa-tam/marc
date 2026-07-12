#include "entropy/tans_tables.hpp"

#include <gtest/gtest.h>

namespace {

TEST(TansTables, BuildsSingleSymbolIdentityAutomaton) {
    marc::entropy::internal::TansDescriptor descriptor{};
    descriptor.frequencies[0x41] = 4096;
    marc::entropy::internal::TansTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(descriptor, tables),
              marc::entropy::internal::TansTableError::none);
    for (std::size_t slot = 0; slot < tables.decode.size(); ++slot) {
        EXPECT_EQ(tables.decode[slot].symbol, 0x41U);
        EXPECT_EQ(tables.decode[slot].bit_count, 0U);
        EXPECT_EQ(tables.decode[slot].state_base, 4096U + slot);
        EXPECT_EQ(tables.encode_states[slot], 4096U + slot);
    }
}

TEST(TansTables, EncodeLookupExactlyInvertsEveryDecodeEntry) {
    marc::entropy::internal::TansDescriptor descriptor{};
    descriptor.frequencies[0x41] = 2731;
    descriptor.frequencies[0x42] = 1365;
    marc::entropy::internal::TansTables tables{};
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(descriptor, tables),
              marc::entropy::internal::TansTableError::none);
    for (std::size_t slot = 0; slot < tables.decode.size(); ++slot) {
        const auto entry = tables.decode[slot];
        const auto q = entry.state_base >> entry.bit_count;
        const auto frequency = descriptor.frequencies[entry.symbol];
        ASSERT_GE(q, frequency);
        ASSERT_LT(q, 2U * frequency);
        const auto index = tables.symbol_offsets[entry.symbol]
            + q - frequency;
        EXPECT_EQ(tables.encode_states[index], 4096U + slot);
        EXPECT_GE(entry.state_base, 4096U);
        EXPECT_LT(entry.state_base + (1U << entry.bit_count), 8193U);
    }
}

TEST(TansTables, IsDeterministicAndTransactionalOnInvalidModel) {
    marc::entropy::internal::TansDescriptor descriptor{};
    descriptor.frequencies[0x41] = 2048;
    descriptor.frequencies[0x42] = 2048;
    marc::entropy::internal::TansTables first{}, second{};
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(descriptor, first),
              marc::entropy::internal::TansTableError::none);
    ASSERT_EQ(marc::entropy::internal::build_tans_tables(descriptor, second),
              marc::entropy::internal::TansTableError::none);
    EXPECT_EQ(first.decode, second.decode);
    EXPECT_EQ(first.encode_states, second.encode_states);
    EXPECT_EQ(first.decode[0].symbol, 0x41U);
    EXPECT_EQ(first.decode[0].bit_count, 1U);
    EXPECT_EQ(first.decode[0].state_base, 4096U);

    const auto saved = first.encode_states;
    descriptor.frequencies[0x42] = 2047;
    EXPECT_EQ(marc::entropy::internal::build_tans_tables(descriptor, first),
              marc::entropy::internal::TansTableError::invalid_frequency_table);
    EXPECT_EQ(first.encode_states, saved);

    descriptor.frequencies[0x42] = 2048;
    descriptor.table_log = 11;
    EXPECT_EQ(marc::entropy::internal::build_tans_tables(descriptor, first),
              marc::entropy::internal::TansTableError::invalid_descriptor);
    EXPECT_EQ(first.encode_states, saved);
}
} // namespace

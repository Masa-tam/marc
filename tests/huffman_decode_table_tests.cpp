#include "entropy/huffman_decode_table.hpp"

#include <gtest/gtest.h>

namespace {

using marc::entropy::internal::HuffmanCodeLengths;
using marc::entropy::internal::HuffmanDecodeStatus;
using marc::entropy::internal::HuffmanDecodeTable;
using marc::entropy::internal::HuffmanTableError;

TEST(HuffmanDecodeTableTest, UsesFastTableForShortCodes) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 2;
    lengths[1] = 2;
    lengths[2] = 2;
    lengths[3] = 2;
    HuffmanDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::none);

    const auto result = marc::entropy::internal::decode_symbol(
        table, 0b10110110, 8);
    EXPECT_EQ(result.status, HuffmanDecodeStatus::symbol);
    EXPECT_EQ(result.symbol, 1);
    EXPECT_EQ(result.bits_consumed, 2);
}

TEST(HuffmanDecodeTableTest, FallsBackForLongCode) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 1;
    for (std::size_t symbol = 1; symbol < 9; ++symbol) {
        lengths[symbol] = static_cast<std::uint8_t>(symbol + 1);
    }
    lengths[9] = 9;
    HuffmanDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::none);

    const auto result = marc::entropy::internal::decode_symbol(
        table, 0b111111111, 9);
    EXPECT_EQ(result.status, HuffmanDecodeStatus::symbol);
    EXPECT_EQ(result.symbol, 9);
    EXPECT_EQ(result.bits_consumed, 9);
}

TEST(HuffmanDecodeTableTest, DecodesEveryCanonicalPhysicalCode) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 1;
    for (std::size_t symbol = 1; symbol < 9; ++symbol) {
        lengths[symbol] = static_cast<std::uint8_t>(symbol + 1);
    }
    lengths[9] = 9;
    marc::entropy::internal::CanonicalHuffmanTable codes{};
    HuffmanDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_canonical_table(lengths, codes),
              HuffmanTableError::none);
    ASSERT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::none);

    for (std::size_t symbol = 0; symbol < 10; ++symbol) {
        const auto result = marc::entropy::internal::decode_symbol(
            table, codes[symbol].lsb_first, codes[symbol].length);
        EXPECT_EQ(result.status, HuffmanDecodeStatus::symbol)
            << "symbol=" << symbol;
        EXPECT_EQ(result.symbol, symbol) << "symbol=" << symbol;
        EXPECT_EQ(result.bits_consumed, codes[symbol].length)
            << "symbol=" << symbol;
    }
}

TEST(HuffmanDecodeTableTest, ReportsNeedInputForCodePrefix) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 1;
    lengths[1] = 2;
    lengths[2] = 3;
    lengths[3] = 3;
    HuffmanDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::none);

    const auto result = marc::entropy::internal::decode_symbol(
        table, 0b11, 2);
    EXPECT_EQ(result.status, HuffmanDecodeStatus::need_input);
    EXPECT_EQ(result.bits_consumed, 2);
}

TEST(HuffmanDecodeTableTest, RejectsMissingSingleSymbolBranch) {
    HuffmanCodeLengths lengths{};
    lengths[0x41] = 1;
    HuffmanDecodeTable table{};
    ASSERT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::none);

    const auto result = marc::entropy::internal::decode_symbol(table, 1, 1);
    EXPECT_EQ(result.status, HuffmanDecodeStatus::invalid_code);
}

TEST(HuffmanDecodeTableTest, RejectsMoreThanVariantMaximumBits) {
    HuffmanDecodeTable table{};
    table.node_count = 1;
    EXPECT_EQ(marc::entropy::internal::decode_symbol(table, 0, 16).status,
              HuffmanDecodeStatus::invalid_argument);
}

TEST(HuffmanDecodeTableTest, DoesNotModifyOutputForInvalidModel) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 2;
    lengths[1] = 2;
    HuffmanDecodeTable table{};
    table.node_count = 7;
    EXPECT_EQ(marc::entropy::internal::build_decode_table(lengths, table),
              HuffmanTableError::incomplete);
    EXPECT_EQ(table.node_count, 7);
}

} // namespace

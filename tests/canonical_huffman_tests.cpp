#include "entropy/canonical_huffman.hpp"

#include <gtest/gtest.h>

namespace {

using marc::entropy::internal::CanonicalHuffmanTable;
using marc::entropy::internal::HuffmanCodeLengths;
using marc::entropy::internal::HuffmanTableError;

TEST(CanonicalHuffmanValidationTest, ControlsWhetherEmptyModelIsAllowed) {
    const HuffmanCodeLengths lengths{};
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::empty_not_allowed);
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths, true),
              HuffmanTableError::none);
}

TEST(CanonicalHuffmanValidationTest, AcceptsOnlyLengthOneForSingleSymbol) {
    HuffmanCodeLengths lengths{};
    lengths[0x41] = 1;
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::none);

    lengths[0x41] = 2;
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::invalid_single_symbol);
}

TEST(CanonicalHuffmanValidationTest, RejectsLengthBeyondVariantLimit) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 16;
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::code_length_exceeded);
}

TEST(CanonicalHuffmanValidationTest, RejectsOversubscribedModel) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 1;
    lengths[1] = 1;
    lengths[2] = 1;
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::oversubscribed);
}

TEST(CanonicalHuffmanValidationTest, RejectsIncompleteMultiSymbolModel) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 2;
    lengths[1] = 2;
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::incomplete);
}

TEST(CanonicalHuffmanTableTest, BuildsOneSymbolPrimitiveVector) {
    HuffmanCodeLengths lengths{};
    lengths[0x41] = 1;
    CanonicalHuffmanTable table{};
    ASSERT_EQ(marc::entropy::internal::build_canonical_table(lengths, table),
              HuffmanTableError::none);
    EXPECT_EQ(table[0x41].length, 1);
    EXPECT_EQ(table[0x41].canonical, 0);
    EXPECT_EQ(table[0x41].lsb_first, 0);
}

TEST(CanonicalHuffmanTableTest, AssignsCanonicalCodesInSymbolOrder) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 2;
    lengths[1] = 2;
    lengths[2] = 2;
    lengths[3] = 2;
    CanonicalHuffmanTable table{};
    ASSERT_EQ(marc::entropy::internal::build_canonical_table(lengths, table),
              HuffmanTableError::none);

    EXPECT_EQ(table[0].canonical, 0b00);
    EXPECT_EQ(table[1].canonical, 0b01);
    EXPECT_EQ(table[2].canonical, 0b10);
    EXPECT_EQ(table[3].canonical, 0b11);
    EXPECT_EQ(table[0].lsb_first, 0b00);
    EXPECT_EQ(table[1].lsb_first, 0b10);
    EXPECT_EQ(table[2].lsb_first, 0b01);
    EXPECT_EQ(table[3].lsb_first, 0b11);
}

TEST(CanonicalHuffmanTableTest, AdvancesCanonicalCodeAcrossLengths) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 1;
    lengths[1] = 2;
    lengths[2] = 2;
    CanonicalHuffmanTable table{};
    ASSERT_EQ(marc::entropy::internal::build_canonical_table(lengths, table),
              HuffmanTableError::none);

    EXPECT_EQ(table[0].canonical, 0b0);
    EXPECT_EQ(table[1].canonical, 0b10);
    EXPECT_EQ(table[2].canonical, 0b11);
    EXPECT_EQ(table[0].lsb_first, 0b0);
    EXPECT_EQ(table[1].lsb_first, 0b01);
    EXPECT_EQ(table[2].lsb_first, 0b11);
}

TEST(CanonicalHuffmanTableTest, LeavesOutputUnchangedWhenValidationFails) {
    HuffmanCodeLengths lengths{};
    lengths[0] = 2;
    lengths[1] = 2;
    CanonicalHuffmanTable table{};
    table[7].canonical = 123;
    EXPECT_EQ(marc::entropy::internal::build_canonical_table(lengths, table),
              HuffmanTableError::incomplete);
    EXPECT_EQ(table[7].canonical, 123);
}

} // namespace

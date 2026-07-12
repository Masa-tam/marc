#include "entropy/canonical_huffman.hpp"

#include <gtest/gtest.h>

namespace {

using marc::entropy::internal::CanonicalHuffmanTable;
using marc::entropy::internal::HuffmanCodeLengths;
using marc::entropy::internal::HuffmanFrequencies;
using marc::entropy::internal::HuffmanBuildError;
using marc::entropy::internal::HuffmanTableError;

TEST(HuffmanFrequencyTest, CountsEveryByteValue) {
    std::array<std::byte, 257> input{};
    for (std::size_t index = 0; index < 256; ++index) {
        input[index] = static_cast<std::byte>(index);
    }
    input[256] = std::byte{0x41};
    HuffmanFrequencies frequencies{};
    ASSERT_EQ(marc::entropy::internal::count_frequencies(input, frequencies),
              HuffmanBuildError::none);
    EXPECT_EQ(frequencies[0], 1U);
    EXPECT_EQ(frequencies[0x41], 2U);
    EXPECT_EQ(frequencies[255], 1U);
}

TEST(LengthLimitedHuffmanTest, HandlesEmptyAndSingleSymbolModels) {
    HuffmanFrequencies frequencies{};
    HuffmanCodeLengths lengths{};
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths),
              HuffmanBuildError::none);
    EXPECT_EQ(lengths, HuffmanCodeLengths{});

    frequencies[0x41] = 7;
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths),
              HuffmanBuildError::none);
    EXPECT_EQ(lengths[0x41], 1);
}

TEST(LengthLimitedHuffmanTest, ProducesHandCheckableOptimalLengths) {
    HuffmanFrequencies frequencies{};
    frequencies[0] = 5;
    frequencies[1] = 7;
    frequencies[2] = 10;
    frequencies[3] = 15;
    frequencies[4] = 20;
    frequencies[5] = 45;
    HuffmanCodeLengths lengths{};
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths),
              HuffmanBuildError::none);
    EXPECT_EQ(lengths[0], 4);
    EXPECT_EQ(lengths[1], 4);
    EXPECT_EQ(lengths[2], 3);
    EXPECT_EQ(lengths[3], 3);
    EXPECT_EQ(lengths[4], 3);
    EXPECT_EQ(lengths[5], 1);
}

TEST(LengthLimitedHuffmanTest, AppliesDeterministicEqualWeightTieBreak) {
    HuffmanFrequencies frequencies{};
    frequencies[0] = 1;
    frequencies[1] = 1;
    frequencies[2] = 1;
    HuffmanCodeLengths lengths{};
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths, 2),
              HuffmanBuildError::none);
    EXPECT_EQ(lengths[0], 2);
    EXPECT_EQ(lengths[1], 2);
    EXPECT_EQ(lengths[2], 1);
}

TEST(LengthLimitedHuffmanTest, RejectsImpossibleAlphabetForLimit) {
    HuffmanFrequencies frequencies{};
    for (std::size_t symbol = 0; symbol < 5; ++symbol) {
        frequencies[symbol] = 1;
    }
    HuffmanCodeLengths lengths{};
    EXPECT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths, 2),
              HuffmanBuildError::impossible_symbol_count);
}

TEST(LengthLimitedHuffmanTest, EnforcesConfiguredMaximumLength) {
    HuffmanFrequencies frequencies{};
    std::uint64_t first = 1;
    std::uint64_t second = 1;
    for (std::size_t symbol = 0; symbol < 20; ++symbol) {
        frequencies[symbol] = first;
        const auto next = first + second;
        first = second;
        second = next;
    }
    HuffmanCodeLengths lengths{};
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths, 15),
              HuffmanBuildError::none);
    for (std::size_t symbol = 0; symbol < 20; ++symbol) {
        EXPECT_GE(lengths[symbol], 1);
        EXPECT_LE(lengths[symbol], 15);
    }
    EXPECT_EQ(marc::entropy::internal::validate_code_lengths(lengths),
              HuffmanTableError::none);
}

TEST(LengthLimitedHuffmanTest, SupportsFullByteAlphabetAtEightBits) {
    HuffmanFrequencies frequencies{};
    frequencies.fill(1);
    HuffmanCodeLengths lengths{};
    ASSERT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths, 8),
              HuffmanBuildError::none);
    for (const auto length : lengths) {
        EXPECT_EQ(length, 8);
    }
}

TEST(LengthLimitedHuffmanTest, LeavesOutputUnchangedOnFailure) {
    HuffmanFrequencies frequencies{};
    frequencies.fill(1);
    HuffmanCodeLengths lengths{};
    lengths[7] = 9;
    EXPECT_EQ(marc::entropy::internal::build_length_limited_code_lengths(
                  frequencies, lengths, 7),
              HuffmanBuildError::impossible_symbol_count);
    EXPECT_EQ(lengths[7], 9);
}

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

#include "dictionary/lz78_format.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(Lz78Format, SerializesAndParsesDefaultParameters) {
    std::array<std::byte, lz78_parameter_size> bytes{};
    ASSERT_EQ(serialize_lz78_parameters({}, {}, bytes), Lz78FormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);

    Lz78Parameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lz78_parameters(bytes, {}, parsed), Lz78FormatError::none);
    EXPECT_EQ(parsed.maximum_entries, 65536U);
    EXPECT_EQ(parsed.flags, 0U);
    EXPECT_EQ(parsed.reserved, 0U);
}

TEST(Lz78Format, RejectsInvalidParametersWithoutPublishing) {
    Lz78Parameters parameters{};
    parameters.maximum_entries = 0;
    EXPECT_EQ(validate_lz78_parameters(parameters, {}),
              Lz78FormatError::invalid_maximum_entries);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lz78_parameters(parameters, {}),
              Lz78FormatError::unknown_flags);
    parameters = {};
    parameters.reserved = 1;
    EXPECT_EQ(validate_lz78_parameters(parameters, {}),
              Lz78FormatError::nonzero_reserved);
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 8;
    EXPECT_EQ(validate_lz78_parameters({}, limits),
              Lz78FormatError::limit_exceeded);

    std::array<std::byte, lz78_parameter_size> bytes{};
    Lz78Parameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lz78_parameters(bytes, {}, parsed),
              Lz78FormatError::invalid_maximum_entries);
    EXPECT_EQ(parsed.maximum_entries, 9U);
}

TEST(Lz78Format, SerializesAndParsesHandVectors) {
    std::array<std::byte, lz78_token_size> bytes{};
    ASSERT_EQ(serialize_lz78_token(
                  {Lz78TokenTag::pair, 0x41, 0}, bytes),
              Lz78FormatError::none);
    constexpr std::array pair{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, pair);

    Lz78Token parsed{Lz78TokenTag::final_index, 9, 9};
    ASSERT_EQ(parse_lz78_token(bytes, parsed), Lz78FormatError::none);
    EXPECT_EQ(parsed.tag, Lz78TokenTag::pair);
    EXPECT_EQ(parsed.symbol, 0x41U);
    EXPECT_EQ(parsed.phrase_index, 0U);

    ASSERT_EQ(serialize_lz78_token(
                  {Lz78TokenTag::final_index, 0, 1}, bytes),
              Lz78FormatError::none);
    constexpr std::array final_index{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, final_index);
}

TEST(Lz78Format, TokenParsingAndSerializationAreTransactional) {
    auto invalid = std::array<std::byte, lz78_token_size>{};
    invalid[0] = std::byte{2};
    Lz78Token parsed{Lz78TokenTag::pair, 9, 9};
    EXPECT_EQ(parse_lz78_token(invalid, parsed), Lz78FormatError::unknown_tag);
    EXPECT_EQ(parsed.symbol, 9U);

    invalid = {};
    invalid[2] = std::byte{1};
    EXPECT_EQ(parse_lz78_token(invalid, parsed),
              Lz78FormatError::nonzero_reserved);
    EXPECT_EQ(parsed.symbol, 9U);

    invalid = {};
    invalid[0] = std::byte{1};
    invalid[1] = std::byte{1};
    EXPECT_EQ(parse_lz78_token(invalid, parsed),
              Lz78FormatError::nonzero_unused_field);
    EXPECT_EQ(parsed.symbol, 9U);

    std::array<std::byte, lz78_token_size> output{};
    output.fill(std::byte{0x7f});
    EXPECT_EQ(serialize_lz78_token(
                  {Lz78TokenTag::final_index, 1, 1}, output),
              Lz78FormatError::nonzero_unused_field);
    EXPECT_EQ(output[0], std::byte{0x7f});
}

} // namespace

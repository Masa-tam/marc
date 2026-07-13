#include "dictionary/lz77_format.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(Lz77Format, SerializesAndParsesDefaultParameters) {
    const Lz77Parameters parameters{};
    std::array<std::byte, lz77_parameter_size> bytes{};
    ASSERT_EQ(serialize_lz77_parameters(parameters, {}, bytes),
              Lz77FormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);
    Lz77Parameters parsed{1, 4, 4, 1};
    EXPECT_EQ(parse_lz77_parameters(bytes, {}, parsed), Lz77FormatError::none);
    EXPECT_EQ(parsed.window_size, 65536U);
    EXPECT_EQ(parsed.max_match_length, 258U);
}

TEST(Lz77Format, RejectsInvalidParametersWithoutPublishing) {
    Lz77Parameters parameters{};
    parameters.min_match_length = 2;
    EXPECT_EQ(validate_lz77_parameters(parameters, {}),
              Lz77FormatError::invalid_match_range);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lz77_parameters(parameters, {}),
              Lz77FormatError::unknown_flags);
    std::array<std::byte, lz77_parameter_size> bytes{};
    Lz77Parameters parsed{9, 9, 9, 9};
    EXPECT_EQ(parse_lz77_parameters(bytes, {}, parsed),
              Lz77FormatError::invalid_window_size);
    EXPECT_EQ(parsed.window_size, 9U);
}

TEST(Lz77Format, SerializesCanonicalTokenForms) {
    std::array<std::byte, lz77_token_size> bytes{};
    Lz77Token token{Lz77TokenTag::literal, 0, 0, 0x41};
    ASSERT_EQ(serialize_lz77_token(token, bytes), Lz77FormatError::none);
    EXPECT_EQ(bytes[0], std::byte{0});
    EXPECT_EQ(bytes[12], std::byte{0x41});
    Lz77Token parsed{};
    EXPECT_EQ(parse_lz77_token(bytes, parsed), Lz77FormatError::none);
    EXPECT_EQ(parsed.literal, 0x41U);

    token = {Lz77TokenTag::terminal_match, 1, 3, 0};
    ASSERT_EQ(serialize_lz77_token(token, bytes), Lz77FormatError::none);
    EXPECT_EQ(bytes[0], std::byte{2});
    EXPECT_EQ(bytes[4], std::byte{1});
    EXPECT_EQ(bytes[8], std::byte{3});
}

TEST(Lz77Format, RejectsReservedAndUnusedTokenFieldsTransactionally) {
    std::array<std::byte, lz77_token_size> bytes{};
    bytes[0] = std::byte{3};
    Lz77Token parsed{Lz77TokenTag::terminal_match, 9, 9, 9};
    EXPECT_EQ(parse_lz77_token(bytes, parsed), Lz77FormatError::unknown_tag);
    EXPECT_EQ(parsed.distance, 9U);
    bytes[0] = std::byte{0};
    bytes[1] = std::byte{1};
    EXPECT_EQ(parse_lz77_token(bytes, parsed),
              Lz77FormatError::nonzero_reserved);
    bytes[1] = std::byte{0};
    bytes[4] = std::byte{1};
    EXPECT_EQ(parse_lz77_token(bytes, parsed),
              Lz77FormatError::nonzero_unused_field);
}

TEST(Lz77Format, ValidatesContextualReferenceAndOverlap) {
    const Lz77Parameters parameters{};
    std::uint64_t output_size{};
    EXPECT_EQ(validate_lz77_token(
                  {Lz77TokenTag::terminal_match, 1, 3, 0}, parameters,
                  {1, 4}, {}, output_size),
              Lz77FormatError::none);
    EXPECT_EQ(output_size, 4U);
    EXPECT_EQ(validate_lz77_token(
                  {Lz77TokenTag::terminal_match, 2, 3, 0}, parameters,
                  {1, 4}, {}, output_size),
              Lz77FormatError::invalid_distance);
    EXPECT_EQ(validate_lz77_token(
                  {Lz77TokenTag::match_then_literal, 1, 3, 0x42}, parameters,
                  {1, 4}, {}, output_size),
              Lz77FormatError::output_size_mismatch);
}
} // namespace

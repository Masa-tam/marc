#include "dictionary/lzss_format.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(LzssFormat, SerializesAndParsesDefaultParameters) {
    std::array<std::byte, lzss_parameter_size> bytes{};
    ASSERT_EQ(serialize_lzss_parameters({}, {}, bytes), LzssFormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x05}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);
    LzssParameters parsed{1, 9, 9, 1};
    EXPECT_EQ(parse_lzss_parameters(bytes, {}, parsed),
              LzssFormatError::none);
    EXPECT_EQ(parsed.window_size, 65536U);
    EXPECT_EQ(parsed.min_match_length, 5U);
    EXPECT_EQ(parsed.max_match_length, 258U);
}

TEST(LzssFormat, RejectsInvalidParametersWithoutPublishing) {
    LzssParameters parameters{};
    parameters.min_match_length = 4;
    EXPECT_EQ(validate_lzss_parameters(parameters, {}),
              LzssFormatError::invalid_match_range);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lzss_parameters(parameters, {}),
              LzssFormatError::unknown_flags);
    marc::core::DecoderLimits limits{};
    limits.max_lz_distance = 1024;
    EXPECT_EQ(validate_lzss_parameters({}, limits),
              LzssFormatError::limit_exceeded);

    std::array<std::byte, lzss_parameter_size> bytes{};
    LzssParameters parsed{9, 9, 9, 9};
    EXPECT_EQ(parse_lzss_parameters(bytes, {}, parsed),
              LzssFormatError::invalid_window_size);
    EXPECT_EQ(parsed.window_size, 9U);
}

TEST(LzssFormat, SerializesAndParsesHandVectors) {
    std::array<std::byte, lzss_match_size> bytes{};
    std::size_t written{};
    ASSERT_EQ(serialize_lzss_token(
                  {LzssTokenTag::literal, 0, 0, 0x41}, bytes, written),
              LzssFormatError::none);
    EXPECT_EQ(written, lzss_literal_size);
    EXPECT_EQ(bytes[0], std::byte{0});
    EXPECT_EQ(bytes[1], std::byte{0x41});

    LzssToken parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_token(bytes, parsed, consumed),
              LzssFormatError::none);
    EXPECT_EQ(consumed, lzss_literal_size);
    EXPECT_EQ(parsed.literal, 0x41U);

    ASSERT_EQ(serialize_lzss_token(
                  {LzssTokenTag::match, 3, 6, 0}, bytes, written),
              LzssFormatError::none);
    constexpr std::array expected{
        std::byte{0x01}, std::byte{0x03}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x06},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);
    ASSERT_EQ(parse_lzss_token(bytes, parsed, consumed),
              LzssFormatError::none);
    EXPECT_EQ(consumed, lzss_match_size);
    EXPECT_EQ(parsed.distance, 3U);
    EXPECT_EQ(parsed.length, 6U);
}

TEST(LzssFormat, TokenParsingIsTransactionalForEveryTruncation) {
    constexpr std::array match{
        std::byte{0x01}, std::byte{0x03}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x06},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    for (std::size_t size = 0; size < match.size(); ++size) {
        LzssToken parsed{LzssTokenTag::match, 9, 9, 9};
        std::size_t consumed = 77;
        EXPECT_EQ(parse_lzss_token(
                      std::span<const std::byte>{match}.first(size),
                      parsed, consumed),
                  LzssFormatError::truncated_token);
        EXPECT_EQ(parsed.distance, 9U);
        EXPECT_EQ(consumed, 77U);
    }
    constexpr std::array literal{std::byte{0x00}};
    LzssToken parsed{LzssTokenTag::match, 9, 9, 9};
    std::size_t consumed = 77;
    EXPECT_EQ(parse_lzss_token(literal, parsed, consumed),
              LzssFormatError::truncated_token);
    EXPECT_EQ(parsed.distance, 9U);
    EXPECT_EQ(consumed, 77U);
}

TEST(LzssFormat, RejectsUnknownAndStructurallyInvalidTokens) {
    constexpr std::array unknown{std::byte{0x02}, std::byte{0x41}};
    LzssToken parsed{LzssTokenTag::match, 9, 9, 9};
    std::size_t consumed = 77;
    EXPECT_EQ(parse_lzss_token(unknown, parsed, consumed),
              LzssFormatError::unknown_tag);
    EXPECT_EQ(parsed.distance, 9U);
    EXPECT_EQ(consumed, 77U);

    std::array<std::byte, lzss_match_size> output{};
    output.fill(std::byte{0x7f});
    std::size_t written = 77;
    EXPECT_EQ(serialize_lzss_token(
                  {LzssTokenTag::literal, 1, 0, 0x41}, output, written),
              LzssFormatError::nonzero_unused_field);
    EXPECT_EQ(output[0], std::byte{0x7f});
    EXPECT_EQ(written, 77U);
    EXPECT_EQ(serialize_lzss_token(
                  {LzssTokenTag::match, 1, 5, 1}, output, written),
              LzssFormatError::nonzero_unused_field);
    EXPECT_EQ(serialize_lzss_token(
                  {LzssTokenTag::match, 1, 5, 0},
                  std::span<std::byte>{output}.first(8), written),
              LzssFormatError::output_too_small);
    EXPECT_EQ(output[0], std::byte{0x7f});
}

TEST(LzssFormat, ValidatesContextualReferenceOverlapAndCostBoundary) {
    std::uint64_t output_size{};
    EXPECT_EQ(validate_lzss_token(
                  {LzssTokenTag::match, 1, 5, 0}, {}, {1, 6}, {},
                  output_size),
              LzssFormatError::none);
    EXPECT_EQ(output_size, 6U);
    EXPECT_EQ(validate_lzss_token(
                  {LzssTokenTag::match, 2, 5, 0}, {}, {1, 6}, {},
                  output_size),
              LzssFormatError::invalid_distance);
    EXPECT_EQ(validate_lzss_token(
                  {LzssTokenTag::match, 1, 4, 0}, {}, {1, 5}, {},
                  output_size),
              LzssFormatError::invalid_length);
    EXPECT_EQ(validate_lzss_token(
                  {LzssTokenTag::match, 1, 6, 0}, {}, {1, 6}, {},
                  output_size),
              LzssFormatError::output_size_mismatch);
}

} // namespace

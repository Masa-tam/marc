#include "dictionary/lzd_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(LzdFormat, SerializesAndParsesDefaultParameters) {
    std::array<std::byte, lzd_parameter_size> bytes{};
    ASSERT_EQ(serialize_lzd_parameters({}, {}, bytes), LzdFormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);

    LzdParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzd_parameters(bytes, {}, parsed), LzdFormatError::none);
    EXPECT_EQ(parsed.maximum_entries, 65536U);
    EXPECT_EQ(parsed.flags, 0U);
    EXPECT_EQ(parsed.reserved, 0U);
}

TEST(LzdFormat, RejectsInvalidParametersWithoutPublishing) {
    LzdParameters parameters{};
    parameters.maximum_entries = 0;
    EXPECT_EQ(validate_lzd_parameters(parameters, {}),
              LzdFormatError::invalid_maximum_entries);
    parameters.maximum_entries = UINT32_MAX;
    EXPECT_EQ(validate_lzd_parameters(parameters, {}),
              LzdFormatError::invalid_maximum_entries);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lzd_parameters(parameters, {}),
              LzdFormatError::unknown_flags);
    parameters = {};
    parameters.reserved = 1;
    EXPECT_EQ(validate_lzd_parameters(parameters, {}),
              LzdFormatError::nonzero_reserved);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 8;
    EXPECT_EQ(validate_lzd_parameters({}, limits),
              LzdFormatError::limit_exceeded);

    std::array<std::byte, lzd_parameter_size> bytes{};
    LzdParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzd_parameters(bytes, {}, parsed),
              LzdFormatError::invalid_maximum_entries);
    EXPECT_EQ(parsed.maximum_entries, 9U);
}

TEST(LzdFormat, AcceptsExactMaximumWithMatchingLocalLimit) {
    LzdParameters parameters{};
    parameters.maximum_entries = lzd_maximum_phrase_entries;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = lzd_maximum_phrase_entries;
    EXPECT_EQ(validate_lzd_parameters(parameters, limits),
              LzdFormatError::none);
}

TEST(LzdFormat, ComputesMaximumTokenExtentTransactionally) {
    std::size_t extent = 99;
    EXPECT_TRUE(lzd_maximum_token_stream_size(0, extent));
    EXPECT_EQ(extent, 0U);
    EXPECT_TRUE(lzd_maximum_token_stream_size(1, extent));
    EXPECT_EQ(extent, 8U);
    EXPECT_TRUE(lzd_maximum_token_stream_size(2, extent));
    EXPECT_EQ(extent, 8U);
    EXPECT_TRUE(lzd_maximum_token_stream_size(3, extent));
    EXPECT_EQ(extent, 16U);
    extent = 99;
    EXPECT_FALSE(lzd_maximum_token_stream_size(UINT64_MAX, extent));
    EXPECT_EQ(extent, 99U);
}

TEST(LzdFormat, SerializesAndParsesHandVectors) {
    std::array<std::byte, lzd_token_size> bytes{};
    ASSERT_EQ(serialize_lzd_token({'A', lzd_absent_reference}, bytes),
              LzdFormatError::none);
    constexpr std::array terminal_a{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
    EXPECT_EQ(bytes, terminal_a);

    LzdToken parsed{9, 9};
    ASSERT_EQ(parse_lzd_token(bytes, parsed), LzdFormatError::none);
    EXPECT_EQ(parsed.left_reference, 'A');
    EXPECT_EQ(parsed.right_reference, lzd_absent_reference);

    ASSERT_EQ(serialize_lzd_token({256, 256}, bytes), LzdFormatError::none);
    constexpr std::array phrase_pair{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, phrase_pair);
}

TEST(LzdFormat, TokenParsingAndSerializationAreTransactional) {
    std::array<std::byte, lzd_token_size> invalid{};
    invalid[0] = std::byte{0xff};
    invalid[1] = std::byte{0xff};
    invalid[2] = std::byte{0xff};
    invalid[3] = std::byte{0xff};
    LzdToken parsed{9, 9};
    EXPECT_EQ(parse_lzd_token(invalid, parsed),
              LzdFormatError::invalid_left_reference);
    EXPECT_EQ(parsed.left_reference, 9U);
    EXPECT_EQ(parsed.right_reference, 9U);

    std::array<std::byte, lzd_token_size> output{};
    output.fill(std::byte{0x7f});
    EXPECT_EQ(serialize_lzd_token(
                  {lzd_absent_reference, 0}, output),
              LzdFormatError::invalid_left_reference);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x7f};
    }));
}

} // namespace

#include "dictionary/lzmw_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(LzmwFormat, SerializesAndParsesDefaultParameters) {
    std::array<std::byte, lzmw_parameter_size> bytes{};
    ASSERT_EQ(serialize_lzmw_parameters({}, {}, bytes),
              LzmwFormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);

    LzmwParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzmw_parameters(bytes, {}, parsed),
              LzmwFormatError::none);
    EXPECT_EQ(parsed.maximum_entries, 65536U);
    EXPECT_EQ(parsed.flags, 0U);
    EXPECT_EQ(parsed.reserved, 0U);
}

TEST(LzmwFormat, RejectsInvalidParametersWithoutPublishing) {
    LzmwParameters parameters{};
    parameters.maximum_entries = 0;
    EXPECT_EQ(validate_lzmw_parameters(parameters, {}),
              LzmwFormatError::invalid_maximum_entries);
    parameters.maximum_entries = UINT32_MAX;
    EXPECT_EQ(validate_lzmw_parameters(parameters, {}),
              LzmwFormatError::invalid_maximum_entries);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lzmw_parameters(parameters, {}),
              LzmwFormatError::unknown_flags);
    parameters = {};
    parameters.reserved = 1;
    EXPECT_EQ(validate_lzmw_parameters(parameters, {}),
              LzmwFormatError::nonzero_reserved);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 8;
    EXPECT_EQ(validate_lzmw_parameters({}, limits),
              LzmwFormatError::limit_exceeded);

    std::array<std::byte, lzmw_parameter_size> bytes{};
    LzmwParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzmw_parameters(bytes, {}, parsed),
              LzmwFormatError::invalid_maximum_entries);
    EXPECT_EQ(parsed.maximum_entries, 9U);
}

TEST(LzmwFormat, AcceptsExactReferenceNamespaceMaximum) {
    LzmwParameters parameters{};
    parameters.maximum_entries = lzmw_maximum_phrase_entries;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = lzmw_maximum_phrase_entries;
    EXPECT_EQ(validate_lzmw_parameters(parameters, limits),
              LzmwFormatError::none);
}

TEST(LzmwFormat, ComputesWorstCaseExtentTransactionally) {
    std::size_t extent = 99;
    EXPECT_TRUE(lzmw_maximum_token_stream_size(0, extent));
    EXPECT_EQ(extent, 0U);
    EXPECT_TRUE(lzmw_maximum_token_stream_size(1, extent));
    EXPECT_EQ(extent, 4U);
    EXPECT_TRUE(lzmw_maximum_token_stream_size(3, extent));
    EXPECT_EQ(extent, 12U);
    extent = 99;
    EXPECT_FALSE(lzmw_maximum_token_stream_size(UINT64_MAX, extent));
    EXPECT_EQ(extent, 99U);
}

TEST(LzmwFormat, SerializesAndParsesReferenceVectors) {
    std::array<std::byte, lzmw_token_size> bytes{};
    ASSERT_EQ(serialize_lzmw_token(256, bytes), LzmwFormatError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);
    std::uint32_t parsed = 9;
    EXPECT_EQ(parse_lzmw_token(bytes, parsed), LzmwFormatError::none);
    EXPECT_EQ(parsed, 256U);

    ASSERT_EQ(serialize_lzmw_token(UINT32_MAX, bytes),
              LzmwFormatError::none);
    EXPECT_TRUE(std::ranges::all_of(bytes, [](const std::byte value) {
        return value == std::byte{0xff};
    }));
    EXPECT_EQ(parse_lzmw_token(bytes, parsed), LzmwFormatError::none);
    EXPECT_EQ(parsed, UINT32_MAX);
}

} // namespace

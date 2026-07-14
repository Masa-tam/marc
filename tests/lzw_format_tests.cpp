#include "dictionary/lzw_format.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace marc::dictionary::internal;

TEST(LzwFormat, SerializesAndParsesDefaultParameters) {
    std::array<std::byte, lzw_parameter_size> bytes{};
    ASSERT_EQ(serialize_lzw_parameters({}, {}, bytes), LzwFormatError::none);
    constexpr std::array expected{
        std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);

    LzwParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzw_parameters(bytes, {}, parsed), LzwFormatError::none);
    EXPECT_EQ(parsed.maximum_code_width, 16U);
    EXPECT_EQ(parsed.flags, 0U);
    EXPECT_EQ(parsed.reserved, 0U);
}

TEST(LzwFormat, RejectsInvalidParametersWithoutPublishing) {
    LzwParameters parameters{};
    parameters.maximum_code_width = 8;
    EXPECT_EQ(validate_lzw_parameters(parameters, {}),
              LzwFormatError::invalid_code_width);
    parameters.maximum_code_width = 25;
    EXPECT_EQ(validate_lzw_parameters(parameters, {}),
              LzwFormatError::invalid_code_width);
    parameters = {};
    parameters.flags = 1;
    EXPECT_EQ(validate_lzw_parameters(parameters, {}),
              LzwFormatError::unknown_flags);
    parameters = {};
    parameters.reserved = 1;
    EXPECT_EQ(validate_lzw_parameters(parameters, {}),
              LzwFormatError::nonzero_reserved);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 255;
    parameters = {};
    parameters.maximum_code_width = 9;
    EXPECT_EQ(validate_lzw_parameters(parameters, limits),
              LzwFormatError::limit_exceeded);

    std::array<std::byte, lzw_parameter_size> bytes{};
    LzwParameters parsed{9, 9, 9};
    EXPECT_EQ(parse_lzw_parameters(bytes, {}, parsed),
              LzwFormatError::invalid_code_width);
    EXPECT_EQ(parsed.maximum_code_width, 9U);
    EXPECT_EQ(parsed.flags, 9U);
    EXPECT_EQ(parsed.reserved, 9U);
}

TEST(LzwFormat, AcceptsDocumentedWidthRangeAndExactLocalLimit) {
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 256;
    LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    EXPECT_EQ(validate_lzw_parameters(parameters, limits),
              LzwFormatError::none);
    EXPECT_EQ(lzw_code_limit(parameters), 512U);

    limits.max_dictionary_entries = (UINT64_C(1) << 24) - 256;
    parameters.maximum_code_width = 24;
    EXPECT_EQ(validate_lzw_parameters(parameters, limits),
              LzwFormatError::none);
    EXPECT_EQ(lzw_code_limit(parameters), UINT32_C(1) << 24);
}

} // namespace

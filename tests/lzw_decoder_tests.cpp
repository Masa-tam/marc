#include "dictionary/lzw_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

std::vector<LzwPhraseEntry> workspace(
    const std::span<const std::byte> input,
    const LzwParameters parameters = {}) {
    return std::vector<LzwPhraseEntry>(
        lzw_validation_workspace_entries(input.size(), parameters));
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

TEST(LzwDecoder, DecodesEmptyAndShortHandVectors) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = decode_lzw_code_stream({}, {}, 0, {}, {}, sentinel);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    constexpr std::array a{std::byte{0x41}, std::byte{0x00}};
    auto phrases = workspace(a);
    std::array<std::byte, 1> output{};
    result = decode_lzw_code_stream(a, {}, 1, {}, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_EQ(result.code_index, 1U);
    EXPECT_EQ(result.input_offset, 2U);
    EXPECT_EQ(result.input_bit_offset, 9U);
    EXPECT_EQ(output[0], std::byte{'A'});

    constexpr std::array aa{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    phrases = workspace(aa);
    std::array<std::byte, 2> aa_output{};
    result = decode_lzw_code_stream(aa, {}, 2, {}, phrases, aa_output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_TRUE(std::equal(aa_output.begin(), aa_output.end(),
                          bytes("AA").begin()));
}

TEST(LzwDecoder, DecodesEveryOneByteValue) {
    for (std::uint32_t value = 0; value < 256; ++value) {
        const std::array input{
            static_cast<std::byte>(value), std::byte{0x00}};
        std::array<std::byte, 1> output{std::byte{0xcc}};
        const auto result = decode_lzw_code_stream(
            input, {}, 1, {}, {}, output);
        ASSERT_EQ(result.error, LzwDecodeError::none) << value;
        EXPECT_EQ(output[0], static_cast<std::byte>(value)) << value;
    }
}

TEST(LzwDecoder, DecodesKwKwKAndNestedPhraseVectors) {
    constexpr std::array aaa{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x02}};
    auto phrases = workspace(aaa);
    std::array<std::byte, 3> aaa_output{};
    auto result = decode_lzw_code_stream(
        aaa, {}, aaa_output.size(), {}, phrases, aaa_output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_TRUE(std::equal(aaa_output.begin(), aaa_output.end(),
                          bytes("AAA").begin()));

    constexpr std::array abababa{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    phrases = workspace(abababa);
    std::array<std::byte, 7> output{};
    result = decode_lzw_code_stream(
        abababa, {}, output.size(), {}, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_EQ(result.code_index, 4U);
    EXPECT_EQ(result.input_bit_offset, 36U);
    EXPECT_TRUE(std::equal(output.begin(), output.end(),
                          bytes("ABABABA").begin()));
}

TEST(LzwDecoder, DecodesBinaryZeroAcrossWidthBoundary) {
    std::vector<std::byte> input(291);
    input[290] = std::byte{0x08};
    auto phrases = workspace(input);
    std::array<std::byte, 259> output{};
    output.fill(std::byte{0xcc});
    const auto result = decode_lzw_code_stream(
        input, {}, output.size(), {}, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_EQ(result.code_index, 258U);
    EXPECT_EQ(result.input_bit_offset, 2324U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{};
    }));
}

TEST(LzwDecoder, DecodesAfterDictionaryFreeze) {
    std::vector<std::byte> input(291);
    LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 256;
    auto phrases = workspace(input, parameters);
    ASSERT_EQ(phrases.size(), 256U);
    std::array<std::byte, 258> output{};
    output.fill(std::byte{0xcc});
    const auto result = decode_lzw_code_stream(
        input, parameters, output.size(), limits, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{};
    }));
}

TEST(LzwDecoder, InvalidInputAndSmallOutputAreAtomic) {
    constexpr std::array forward{
        std::byte{0x41}, std::byte{0x02}, std::byte{0x02}};
    auto phrases = workspace(forward);
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzw_code_stream(
        forward, {}, output.size(), {}, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::invalid_code_stream);
    EXPECT_EQ(result.code_index, 1U);
    EXPECT_EQ(result.input_offset, 1U);
    EXPECT_EQ(result.input_bit_offset, 9U);
    EXPECT_EQ(result.format_error, LzwFormatError::invalid_code);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    constexpr std::array nonzero_padding{
        std::byte{0x41}, std::byte{0x02}};
    output.fill(std::byte{0xcc});
    result = decode_lzw_code_stream(
        nonzero_padding, {}, 1, {}, {}, output);
    EXPECT_EQ(result.error, LzwDecodeError::invalid_code_stream);
    EXPECT_EQ(result.format_error, LzwFormatError::nonzero_padding);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    constexpr std::array a{std::byte{0x41}, std::byte{0x00}};
    phrases = workspace(a);
    result = decode_lzw_code_stream(a, {}, 1, {}, phrases, {});
    EXPECT_EQ(result.error, LzwDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzwDecoder, EnforcesWorkspaceAndConfigurationLimits) {
    constexpr std::array aa{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    std::array<std::byte, 2> output{};
    auto result = decode_lzw_code_stream(aa, {}, 2, {}, {}, output);
    EXPECT_EQ(result.error, LzwDecodeError::invalid_code_stream);
    EXPECT_EQ(result.validation_error,
              LzwValidationError::workspace_too_small);

    auto phrases = workspace(aa);
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 2;
    result = decode_lzw_code_stream(
        aa, {}, 2, limits, phrases, output);
    EXPECT_EQ(result.error, LzwDecodeError::invalid_code_stream);
    EXPECT_EQ(result.validation_error, LzwValidationError::limit_exceeded);
}

} // namespace

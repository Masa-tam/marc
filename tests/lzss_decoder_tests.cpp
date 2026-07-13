#include "dictionary/lzss_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const LzssToken token) {
    std::array<std::byte, lzss_match_size> encoded{};
    std::size_t written{};
    ASSERT_EQ(serialize_lzss_token(token, encoded, written),
              LzssFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + written);
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

TEST(LzssDecoder, DecodesEmptyAndLiteralFrames) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = decode_lzss_token_stream({}, {}, 0, {}, sentinel);
    EXPECT_EQ(result.error, LzssDecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(result.input_offset, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    std::vector<std::byte> tokens;
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    std::array<std::byte, 1> output{};
    result = decode_lzss_token_stream(tokens, {}, 1, {}, output);
    EXPECT_EQ(result.error, LzssDecodeError::none);
    EXPECT_EQ(result.input_offset, 2U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssDecoder, DecodesHandCheckableOverlapVector) {
    std::vector<std::byte> tokens;
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    append(tokens, {LzssTokenTag::match, 1, 5, 0});
    std::array<std::byte, 6> output{};
    const auto result = decode_lzss_token_stream(tokens, {}, 6, {}, output);
    EXPECT_EQ(result.error, LzssDecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(),
                          bytes("AAAAAA").begin()));
}

TEST(LzssDecoder, DecodesMatchFollowedByLiteralVector) {
    std::vector<std::byte> tokens;
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    append(tokens, {LzssTokenTag::literal, 0, 0, 'B'});
    append(tokens, {LzssTokenTag::literal, 0, 0, 'C'});
    append(tokens, {LzssTokenTag::match, 3, 6, 0});
    append(tokens, {LzssTokenTag::literal, 0, 0, 'X'});
    std::array<std::byte, 10> output{};
    const auto result = decode_lzss_token_stream(tokens, {}, 10, {}, output);
    EXPECT_EQ(result.error, LzssDecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(),
                          bytes("ABCABCABCX").begin()));
}

TEST(LzssDecoder, InvalidInputAndSmallOutputAreAtomic) {
    std::vector<std::byte> tokens;
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    append(tokens, {LzssTokenTag::match, 2, 5, 0});
    std::array<std::byte, 6> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzss_token_stream(tokens, {}, 6, {}, output);
    EXPECT_EQ(result.error, LzssDecodeError::invalid_token_stream);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 2U);
    EXPECT_EQ(result.format_error, LzssFormatError::invalid_distance);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    tokens.clear();
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    output.fill(std::byte{0xcc});
    result = decode_lzss_token_stream(
        tokens, {}, 1, {}, std::span<std::byte>{output}.first(0));
    EXPECT_EQ(result.error, LzssDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssDecoder, EnforcesDictionaryAndConfigurationLimits) {
    std::vector<std::byte> tokens;
    append(tokens, {LzssTokenTag::literal, 0, 0, 'A'});
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 1;
    std::array<std::byte, 1> output{};
    auto result = decode_lzss_token_stream(tokens, {}, 1, limits, output);
    EXPECT_EQ(result.error, LzssDecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, LzssValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = limits.max_total_output_size + 1;
    result = decode_lzss_token_stream(tokens, {}, 1, limits, output);
    EXPECT_EQ(result.error, LzssDecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, LzssValidationError::limit_exceeded);
}

} // namespace

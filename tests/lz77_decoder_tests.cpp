#include "dictionary/lz77_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const Lz77Token token) {
    std::array<std::byte, lz77_token_size> encoded{};
    ASSERT_EQ(serialize_lz77_token(token, encoded), Lz77FormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

TEST(Lz77Decoder, DecodesEmptyAndLiteralFrames) {
    std::array<std::byte, 1> sentinel{std::byte{0xCC}};
    auto result = decode_lz77_token_stream({}, {}, 0, {}, sentinel);
    EXPECT_EQ(result.error, Lz77DecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xCC});

    std::vector<std::byte> tokens;
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    std::array<std::byte, 1> output{};
    result = decode_lz77_token_stream(tokens, {}, 1, {}, output);
    EXPECT_EQ(result.error, Lz77DecodeError::none);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(Lz77Decoder, DecodesHandCheckableOverlapVector) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    append(tokens, {Lz77TokenTag::terminal_match, 1, 3, 0});
    std::array<std::byte, 4> output{};
    const auto result = decode_lz77_token_stream(tokens, {}, 4, {}, output);
    EXPECT_EQ(result.error, Lz77DecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(), bytes("AAAA").begin()));
}

TEST(Lz77Decoder, DecodesMatchThenLiteralVector) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'B'});
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'C'});
    append(tokens, {Lz77TokenTag::match_then_literal, 3, 3, 'X'});
    std::array<std::byte, 7> output{};
    const auto result = decode_lz77_token_stream(tokens, {}, 7, {}, output);
    EXPECT_EQ(result.error, Lz77DecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(), bytes("ABCABCX").begin()));
}

TEST(Lz77Decoder, InvalidInputAndSmallOutputAreAtomic) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    append(tokens, {Lz77TokenTag::terminal_match, 2, 3, 0});
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xCC});
    auto result = decode_lz77_token_stream(tokens, {}, 4, {}, output);
    EXPECT_EQ(result.error, Lz77DecodeError::invalid_token_stream);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.format_error, Lz77FormatError::invalid_distance);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(),
                            [](std::byte value) { return value == std::byte{0xCC}; }));

    tokens.clear();
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    result = decode_lz77_token_stream(tokens, {}, 1, {}, {});
    EXPECT_EQ(result.error, Lz77DecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 1U);
}

TEST(Lz77Decoder, EnforcesDictionaryAndConfigurationLimits) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz77TokenTag::literal, 0, 0, 'A'});
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 15;
    std::array<std::byte, 1> output{};
    auto result = decode_lz77_token_stream(tokens, {}, 1, limits, output);
    EXPECT_EQ(result.error, Lz77DecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, Lz77ValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = limits.max_total_output_size + 1;
    result = decode_lz77_token_stream(tokens, {}, 1, limits, output);
    EXPECT_EQ(result.error, Lz77DecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, Lz77ValidationError::limit_exceeded);
}

} // namespace

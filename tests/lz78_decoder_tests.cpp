#include "dictionary/lz78_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const Lz78Token token) {
    std::array<std::byte, lz78_token_size> encoded{};
    ASSERT_EQ(serialize_lz78_token(token, encoded), Lz78FormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

TEST(Lz78Decoder, DecodesEmptyPairAndFinalIndexVectors) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = decode_lz78_token_stream({}, {}, 0, {}, {}, sentinel);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    std::array<Lz78PhraseEntry, 2> workspace{};
    std::array<std::byte, 2> output{};
    result = decode_lz78_token_stream(
        tokens, {}, 1, {}, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_EQ(output[0], std::byte{'A'});

    append(tokens, {Lz78TokenTag::final_index, 0, 1});
    result = decode_lz78_token_stream(
        tokens, {}, 2, {}, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_EQ(result.token_index, 2U);
    EXPECT_EQ(result.input_offset, 16U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{'A'});
}

TEST(Lz78Decoder, DecodesNestedPhraseChainsInForwardOrder) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    append(tokens, {Lz78TokenTag::pair, 'B', 1});
    append(tokens, {Lz78TokenTag::pair, 'C', 2});
    append(tokens, {Lz78TokenTag::final_index, 0, 3});
    std::array<Lz78PhraseEntry, 4> workspace{};
    std::array<std::byte, 9> output{};
    const auto result = decode_lz78_token_stream(
        tokens, {}, output.size(), {}, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(),
                          bytes("AABABCABC").begin()));
}

TEST(Lz78Decoder, DecodesPairEndingFrameAndBinaryZero) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    append(tokens, {Lz78TokenTag::pair, 'B', 0});
    append(tokens, {Lz78TokenTag::pair, 'B', 1});
    std::array<Lz78PhraseEntry, 3> workspace{};
    std::array<std::byte, 4> output{};
    auto result = decode_lz78_token_stream(
        tokens, {}, output.size(), {}, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(), bytes("ABAB").begin()));

    tokens.clear();
    append(tokens, {Lz78TokenTag::pair, 0, 0});
    std::array<std::byte, 1> zero_output{std::byte{0xff}};
    result = decode_lz78_token_stream(
        tokens, {}, 1, {}, workspace, zero_output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_EQ(zero_output[0], std::byte{0});
}

TEST(Lz78Decoder, DecodesWithFrozenDictionary) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    append(tokens, {Lz78TokenTag::pair, 'A', 1});
    Lz78Parameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<Lz78PhraseEntry, 1> workspace{};
    std::array<std::byte, 3> output{};
    const auto result = decode_lz78_token_stream(
        tokens, parameters, output.size(), limits, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::none);
    EXPECT_TRUE(std::equal(output.begin(), output.end(), bytes("AAA").begin()));
}

TEST(Lz78Decoder, InvalidInputAndSmallOutputAreAtomic) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    append(tokens, {Lz78TokenTag::pair, 'B', 2});
    std::array<Lz78PhraseEntry, 2> workspace{};
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lz78_token_stream(
        tokens, {}, 2, {}, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::invalid_token_stream);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 8U);
    EXPECT_EQ(result.format_error, Lz78FormatError::invalid_phrase_index);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    tokens.resize(lz78_token_size);
    output.fill(std::byte{0xcc});
    result = decode_lz78_token_stream(
        tokens, {}, 1, {}, workspace, std::span<std::byte>{});
    EXPECT_EQ(result.error, Lz78DecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(Lz78Decoder, EnforcesWorkspaceAndConfigurationLimits) {
    std::vector<std::byte> tokens;
    append(tokens, {Lz78TokenTag::pair, 'A', 0});
    std::array<std::byte, 1> output{};
    auto result = decode_lz78_token_stream(tokens, {}, 1, {}, {}, output);
    EXPECT_EQ(result.error, Lz78DecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error,
              Lz78ValidationError::workspace_too_small);

    std::array<Lz78PhraseEntry, 1> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    result = decode_lz78_token_stream(
        tokens, {}, 1, limits, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, Lz78ValidationError::limit_exceeded);

    limits = {};
    limits.max_frame_size = limits.max_total_output_size + 1;
    result = decode_lz78_token_stream(
        tokens, {}, 1, limits, workspace, output);
    EXPECT_EQ(result.error, Lz78DecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, Lz78ValidationError::limit_exceeded);
}

} // namespace

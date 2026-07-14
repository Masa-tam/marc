#include "dictionary/lzd_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const LzdToken token) {
    std::array<std::byte, lzd_token_size> encoded{};
    ASSERT_EQ(serialize_lzd_token(token, encoded), LzdFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

LzdDecodeResult decode(const std::span<const std::byte> input,
                       const std::span<std::byte> output,
                       const LzdParameters parameters = {},
                       const marc::core::DecoderLimits limits = {}) {
    std::vector<LzdPhraseEntry> phrases(
        lzd_validation_workspace_entries(input.size(), parameters));
    std::vector<std::uint32_t> expansion(
        lzd_expansion_workspace_entries(phrases.size(), !output.empty()));
    return decode_lzd_token_stream(
        input, parameters, output.size(), limits, phrases, expansion, output);
}

TEST(LzdDecoder, DecodesEmptyAndHandVectors) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = decode_lzd_token_stream(
        {}, {}, 0, {}, {}, {}, sentinel);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(result.expansion_workspace_entries, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    std::vector<std::byte> tokens;
    append(tokens, {'A', lzd_absent_reference});
    std::array<std::byte, 1> output{};
    result = decode(tokens, output);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_EQ(output[0], std::byte{'A'});

    tokens.clear();
    append(tokens, {'A', 'B'});
    append(tokens, {'A', lzd_absent_reference});
    std::array<std::byte, 3> aba{};
    result = decode(tokens, aba);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(aba, bytes("ABA")));

    tokens.clear();
    append(tokens, {'A', 'B'});
    append(tokens, {256, lzd_absent_reference});
    std::array<std::byte, 4> abab{};
    result = decode(tokens, abab);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(abab, bytes("ABAB")));

    tokens.clear();
    append(tokens, {'A', 'B'});
    append(tokens, {256, 256});
    std::array<std::byte, 6> ababab{};
    result = decode(tokens, ababab);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(ababab, bytes("ABABAB")));
}

TEST(LzdDecoder, DecodesPublishedExampleWithoutSentinel) {
    std::vector<std::byte> tokens;
    append(tokens, {'a', 'b'});
    append(tokens, {'b', 'a'});
    append(tokens, {256, 256});
    append(tokens, {'a', 256});
    append(tokens, {'a', lzd_absent_reference});
    std::array<std::byte, 12> output{};
    const auto result = decode(tokens, output);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_EQ(result.token_index, 5U);
    EXPECT_EQ(result.input_offset, 40U);
    EXPECT_TRUE(std::ranges::equal(output, bytes("abbaababaaba")));
}

TEST(LzdDecoder, ExpandsDeepBinaryGrammarIteratively) {
    std::vector<std::byte> tokens;
    append(tokens, {'A', 'A'});
    append(tokens, {256, 'B'});
    append(tokens, {257, 'C'});
    append(tokens, {258, lzd_absent_reference});
    std::array<LzdPhraseEntry, 4> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, 13> output{};
    const auto result = decode_lzd_token_stream(
        tokens, {}, output.size(), {}, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_EQ(result.expansion_workspace_entries, 4U);
    EXPECT_TRUE(std::ranges::equal(output, bytes("AAAABAABCAABC")));
}

TEST(LzdDecoder, DecodesWithFrozenDictionary) {
    std::vector<std::byte> tokens;
    append(tokens, {'A', 'B'});
    append(tokens, {256, 256});
    LzdParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 6> output{};
    const auto result = decode_lzd_token_stream(
        tokens, parameters, output.size(), limits, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(output, bytes("ABABAB")));
}

TEST(LzdDecoder, InvalidInputAndCapacityFailuresAreAtomic) {
    std::vector<std::byte> tokens;
    append(tokens, {'A', 'B'});
    append(tokens, {257, 'C'});
    std::array<LzdPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzd_token_stream(
        tokens, {}, output.size(), {}, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::invalid_token_stream);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.input_offset, 8U);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_phrase_reference);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    tokens.resize(lzd_token_size);
    output.fill(std::byte{0xcc});
    result = decode_lzd_token_stream(
        tokens, {}, 2, {}, phrases, expansion,
        std::span<std::byte>{output}.first(1));
    EXPECT_EQ(result.error, LzdDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 2U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    output.fill(std::byte{0xcc});
    result = decode_lzd_token_stream(
        tokens, {}, 2, {}, phrases, {}, output);
    EXPECT_EQ(result.error, LzdDecodeError::expansion_workspace_too_small);
    EXPECT_EQ(result.expansion_workspace_entries, 2U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzdDecoder, EnforcesPhraseWorkspaceAndLocalLimits) {
    std::vector<std::byte> tokens;
    append(tokens, {'A', 'B'});
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 2> output{};
    auto result = decode_lzd_token_stream(
        tokens, {}, output.size(), {}, {}, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error,
              LzdValidationError::workspace_too_small);

    std::array<LzdPhraseEntry, 1> phrases{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    result = decode_lzd_token_stream(
        tokens, {}, output.size(), limits, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error, LzdValidationError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        tokens.size() + sizeof(LzdPhraseEntry)
        + 2 * sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    output.fill(std::byte{0xcc});
    result = decode_lzd_token_stream(
        tokens, {}, output.size(), limits, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdDecodeError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

} // namespace

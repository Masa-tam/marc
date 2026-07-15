#include "dictionary/lzmw_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

void append(std::vector<std::byte>& bytes, const std::uint32_t reference) {
    std::array<std::byte, lzmw_token_size> encoded{};
    ASSERT_EQ(serialize_lzmw_token(reference, encoded),
              LzmwFormatError::none);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

LzmwDecodeResult decode(const std::span<const std::byte> input,
                        const std::span<std::byte> output,
                        const LzmwParameters parameters = {},
                        const marc::core::DecoderLimits limits = {}) {
    std::vector<LzmwPhraseEntry> phrases(
        lzmw_validation_workspace_entries(input.size(), parameters));
    std::vector<std::uint32_t> expansion(
        lzmw_expansion_workspace_entries(phrases.size(), !output.empty()));
    return decode_lzmw_token_stream(
        input, parameters, output.size(), limits, phrases, expansion, output);
}

TEST(LzmwDecoder, DecodesEmptyLiteralAndAbabVectors) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = decode_lzmw_token_stream({}, {}, 0, {}, {}, {}, sentinel);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(result.expansion_workspace_entries, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    std::vector<std::byte> tokens;
    append(tokens, 'A');
    std::array<std::byte, 1> a{};
    result = decode(tokens, a);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_EQ(a[0], std::byte{'A'});

    append(tokens, 'B');
    append(tokens, 256);
    std::array<std::byte, 4> abab{};
    result = decode(tokens, abab);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_EQ(result.token_index, 3U);
    EXPECT_EQ(result.input_offset, 12U);
    EXPECT_TRUE(std::ranges::equal(abab, bytes("ABAB")));
}

TEST(LzmwDecoder, DecodesPublishedFactorization) {
    std::vector<std::byte> tokens;
    for (const auto reference : {97U, 98U, 98U, 97U, 256U, 256U, 259U, 97U})
        append(tokens, reference);
    std::array<std::byte, 12> output{};
    const auto result = decode(tokens, output);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_EQ(result.expansion_workspace_entries, 8U);
    EXPECT_TRUE(std::ranges::equal(output, bytes("abbaababaaba")));
}

TEST(LzmwDecoder, ExpandsGrowingGrammarIteratively) {
    std::vector<std::byte> tokens;
    for (const auto reference : {65U, 65U, 256U, 257U, 258U})
        append(tokens, reference);
    std::array<LzmwPhraseEntry, 4> phrases{};
    std::array<std::uint32_t, 5> expansion{};
    std::array<std::byte, 12> output{};
    const auto result = decode_lzmw_token_stream(
        tokens, {}, output.size(), {}, phrases, expansion, output);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_EQ(result.expansion_workspace_entries, 5U);
    EXPECT_TRUE(std::ranges::equal(output, bytes("AAAAAAAAAAAA")));
}

TEST(LzmwDecoder, DecodesWithFrozenDictionary) {
    std::vector<std::byte> tokens;
    for (const auto reference : {65U, 66U, 256U, 256U})
        append(tokens, reference);
    LzmwParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 6> output{};
    const auto result = decode_lzmw_token_stream(
        tokens, parameters, output.size(), limits, phrases, expansion, output);
    EXPECT_EQ(result.error, LzmwDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(output, bytes("ABABAB")));
}

TEST(LzmwDecoder, InvalidInputAndCapacityFailuresAreAtomic) {
    std::vector<std::byte> tokens;
    append(tokens, 'A');
    append(tokens, 'B');
    append(tokens, 257);
    std::array<LzmwPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzmw_token_stream(
        tokens, {}, output.size(), {}, phrases, expansion, output);
    EXPECT_EQ(result.error, LzmwDecodeError::invalid_token_stream);
    EXPECT_EQ(result.token_index, 2U);
    EXPECT_EQ(result.input_offset, 8U);
    EXPECT_EQ(result.format_error,
              LzmwFormatError::invalid_phrase_reference);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    tokens.resize(2 * lzmw_token_size);
    append(tokens, 256);
    output.fill(std::byte{0xcc});
    result = decode_lzmw_token_stream(
        tokens, {}, output.size(), {}, phrases, expansion,
        std::span<std::byte>{output}.first(3));
    EXPECT_EQ(result.error, LzmwDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 4U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    output.fill(std::byte{0xcc});
    result = decode_lzmw_token_stream(
        tokens, {}, output.size(), {}, phrases,
        std::span<std::uint32_t>{expansion}.first(2), output);
    EXPECT_EQ(result.error, LzmwDecodeError::expansion_workspace_too_small);
    EXPECT_EQ(result.expansion_workspace_entries, 3U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzmwDecoder, EnforcesPhraseWorkspaceAndAggregateLimit) {
    std::vector<std::byte> tokens;
    append(tokens, 'A');
    append(tokens, 'B');
    append(tokens, 256);
    std::array<LzmwPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzmw_token_stream(
        tokens, {}, output.size(), {}, {}, expansion, output);
    EXPECT_EQ(result.error, LzmwDecodeError::invalid_token_stream);
    EXPECT_EQ(result.validation_error,
              LzmwValidationError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = tokens.size()
        + phrases.size() * sizeof(LzmwPhraseEntry)
        + expansion.size() * sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = decode_lzmw_token_stream(
        tokens, {}, output.size(), limits, phrases, expansion, output);
    EXPECT_EQ(result.error, LzmwDecodeError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

} // namespace

#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

std::vector<std::byte> encode(
    const std::span<const std::byte> input,
    const LzmwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzmwEncoderEntry> workspace(
        lzmw_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lzmw_token_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, LzmwEncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lzmw_token_stream(
                  input, parameters, limits, workspace, output).error,
              LzmwEncodeError::none);
    return output;
}

std::vector<std::byte> encode(const std::string_view text) {
    return encode(bytes(text));
}

void expect_reference(const std::span<const std::byte> encoded,
                      const std::size_t index,
                      const std::uint32_t expected) {
    ASSERT_GE(encoded.size(), (index + 1) * lzmw_token_size);
    std::uint32_t reference{};
    const std::span<const std::byte, lzmw_token_size> source{
        encoded.data() + index * lzmw_token_size, lzmw_token_size};
    ASSERT_EQ(parse_lzmw_token(source, reference), LzmwFormatError::none);
    EXPECT_EQ(reference, expected);
}

TEST(LzmwEncoder, EmitsHandCheckableVectors) {
    EXPECT_TRUE(encode("").empty());
    const auto a = encode("A");
    ASSERT_EQ(a.size(), lzmw_token_size);
    expect_reference(a, 0, 'A');
    const auto ab = encode("AB");
    ASSERT_EQ(ab.size(), 2 * lzmw_token_size);
    expect_reference(ab, 0, 'A');
    expect_reference(ab, 1, 'B');
    const auto abab = encode("ABAB");
    ASSERT_EQ(abab.size(), 3 * lzmw_token_size);
    expect_reference(abab, 0, 'A');
    expect_reference(abab, 1, 'B');
    expect_reference(abab, 2, 256);
}

TEST(LzmwEncoder, EncodesEveryOneByteValueCanonically) {
    for (unsigned int value = 0; value < 256; ++value) {
        const std::array input{static_cast<std::byte>(value)};
        const auto encoded = encode(input);
        ASSERT_EQ(encoded.size(), lzmw_token_size);
        expect_reference(encoded, 0, value);
    }
}

TEST(LzmwEncoder, EmitsPublishedFactorization) {
    const auto encoded = encode("abbaababaaba");
    constexpr std::array expected{
        97U, 98U, 98U, 97U, 256U, 256U, 259U, 97U};
    ASSERT_EQ(encoded.size(), expected.size() * lzmw_token_size);
    for (std::size_t index = 0; index < expected.size(); ++index)
        expect_reference(encoded, index, expected[index]);
}

TEST(LzmwEncoder, FreezesWithoutResettingMatches) {
    const auto input = bytes("ABABAB");
    LzmwParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    const auto encoded = encode(input, parameters, limits);
    constexpr std::array expected{65U, 66U, 256U, 256U};
    ASSERT_EQ(encoded.size(), expected.size() * lzmw_token_size);
    for (std::size_t index = 0; index < expected.size(); ++index)
        expect_reference(encoded, index, expected[index]);
}

TEST(LzmwEncoder, PlansDeterministicallyAndRoundTripsBinaryData) {
    std::vector<std::byte> input;
    for (unsigned int value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());
    std::vector<LzmwEncoderEntry> workspace(
        lzmw_encoder_workspace_entries(input.size(), {}));
    const auto first = plan_lzmw_token_stream(input, {}, {}, workspace);
    ASSERT_EQ(first.error, LzmwEncodeError::none);
    const auto second = plan_lzmw_token_stream(input, {}, {}, workspace);
    EXPECT_EQ(second.output_size, first.output_size);
    EXPECT_EQ(second.token_count, first.token_count);
    EXPECT_EQ(second.dictionary_entries, first.dictionary_entries);
    std::vector<std::byte> encoded(first.output_size);
    ASSERT_EQ(encode_lzmw_token_stream(
                  input, {}, {}, workspace, encoded).error,
              LzmwEncodeError::none);
    std::vector<std::byte> encoded_again(first.output_size);
    ASSERT_EQ(encode_lzmw_token_stream(
                  input, {}, {}, workspace, encoded_again).error,
              LzmwEncodeError::none);
    EXPECT_EQ(encoded_again, encoded);
    std::vector<LzmwPhraseEntry> phrases(
        lzmw_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::uint32_t> expansion(
        lzmw_expansion_workspace_entries(phrases.size(), true));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzmw_token_stream(
                  encoded, {}, input.size(), {}, phrases, expansion, decoded)
                  .error,
              LzmwDecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(LzmwEncoder, RoundTripsDeterministicPseudoRandomDataWithinTokenBound) {
    std::vector<std::byte> input(1025);
    std::uint32_t state = 0x4d415243;
    for (auto& value : input) {
        state = state * 1664525U + 1013904223U;
        value = static_cast<std::byte>(state >> 24);
    }
    const auto encoded = encode(input);
    EXPECT_LE(encoded.size(), input.size() * lzmw_token_size);
    std::vector<LzmwPhraseEntry> phrases(
        lzmw_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::uint32_t> expansion(
        lzmw_expansion_workspace_entries(phrases.size(), true));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzmw_token_stream(
                  encoded, {}, input.size(), {}, phrases, expansion, decoded)
                  .error,
              LzmwDecodeError::none);
    EXPECT_EQ(decoded, input);
    EXPECT_EQ(encode(input), encoded);
}

TEST(LzmwEncoder, ExpectedFailuresAreAtomic) {
    const auto input = bytes("ABAB");
    std::array<LzmwEncoderEntry, 3> workspace{};
    std::array<std::byte, 11> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzmw_token_stream(
        input, {}, {}, workspace, output);
    EXPECT_EQ(result.error, LzmwEncodeError::output_too_small);
    EXPECT_EQ(result.output_size, 12U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    result = encode_lzmw_token_stream(input, {}, {}, {}, output);
    EXPECT_EQ(result.error, LzmwEncodeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 11;
    result = encode_lzmw_token_stream(
        input, {}, limits, workspace, output);
    EXPECT_EQ(result.error, LzmwEncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    LzmwParameters parameters{};
    parameters.maximum_entries = 0;
    result = encode_lzmw_token_stream(
        input, parameters, {}, workspace, output);
    EXPECT_EQ(result.error, LzmwEncodeError::invalid_parameters);
    EXPECT_EQ(result.format_error,
              LzmwFormatError::invalid_maximum_entries);
}

TEST(LzmwEncoder, EnforcesInputAndAggregateWorkspaceLimits) {
    const auto input = bytes("ABAB");
    std::array<LzmwEncoderEntry, 3> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = input.size() - 1;
    auto result = plan_lzmw_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzmwEncodeError::input_limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes =
        input.size() + 3 * sizeof(LzmwEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = plan_lzmw_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzmwEncodeError::workspace_limit_exceeded);
}

} // namespace

#include "dictionary/lzd_decoder.hpp"
#include "dictionary/lzd_encoder.hpp"

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
    const LzdParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzdEncoderEntry> workspace(
        lzd_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lzd_token_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, LzdEncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lzd_token_stream(
                  input, parameters, limits, workspace, output).error,
              LzdEncodeError::none);
    return output;
}

std::vector<std::byte> encode(const std::string_view text) {
    return encode(bytes(text));
}

void expect_token(const std::span<const std::byte> encoded,
                  const std::size_t index, const LzdToken expected) {
    ASSERT_GE(encoded.size(), (index + 1) * lzd_token_size);
    LzdToken token{};
    const std::span<const std::byte, lzd_token_size> source{
        encoded.data() + index * lzd_token_size, lzd_token_size};
    ASSERT_EQ(parse_lzd_token(source, token), LzdFormatError::none);
    EXPECT_EQ(token.left_reference, expected.left_reference);
    EXPECT_EQ(token.right_reference, expected.right_reference);
}

TEST(LzdEncoder, EmitsHandCheckableVectors) {
    EXPECT_TRUE(encode("").empty());

    const auto a = encode("A");
    ASSERT_EQ(a.size(), lzd_token_size);
    expect_token(a, 0, {'A', lzd_absent_reference});

    const auto ab = encode("AB");
    ASSERT_EQ(ab.size(), lzd_token_size);
    expect_token(ab, 0, {'A', 'B'});

    const auto aba = encode("ABA");
    ASSERT_EQ(aba.size(), 2 * lzd_token_size);
    expect_token(aba, 0, {'A', 'B'});
    expect_token(aba, 1, {'A', lzd_absent_reference});

    const auto abab = encode("ABAB");
    ASSERT_EQ(abab.size(), 2 * lzd_token_size);
    expect_token(abab, 0, {'A', 'B'});
    expect_token(abab, 1, {256, lzd_absent_reference});

    const auto ababab = encode("ABABAB");
    ASSERT_EQ(ababab.size(), 2 * lzd_token_size);
    expect_token(ababab, 0, {'A', 'B'});
    expect_token(ababab, 1, {256, 256});
}

TEST(LzdEncoder, EncodesEveryOneByteValueCanonically) {
    for (unsigned int value = 0; value < 256; ++value) {
        const std::array input{static_cast<std::byte>(value)};
        const auto encoded = encode(input);
        ASSERT_EQ(encoded.size(), lzd_token_size);
        expect_token(encoded, 0,
                     {value, lzd_absent_reference});
    }
}

TEST(LzdEncoder, EmitsPublishedFactorization) {
    const auto encoded = encode("abbaababaaba");
    ASSERT_EQ(encoded.size(), 5 * lzd_token_size);
    expect_token(encoded, 0, {'a', 'b'});
    expect_token(encoded, 1, {'b', 'a'});
    expect_token(encoded, 2, {256, 256});
    expect_token(encoded, 3, {'a', 256});
    expect_token(encoded, 4, {'a', lzd_absent_reference});
}

TEST(LzdEncoder, FreezesWithoutResettingMatches) {
    const auto input = bytes("ABABABABAB");
    LzdParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    const auto encoded = encode(input, parameters, limits);
    ASSERT_EQ(encoded.size(), 3 * lzd_token_size);
    expect_token(encoded, 0, {'A', 'B'});
    expect_token(encoded, 1, {256, 256});
    expect_token(encoded, 2, {256, 256});
}

TEST(LzdEncoder, PlansDeterministicallyAndRoundTripsBinaryData) {
    std::vector<std::byte> input;
    for (unsigned int value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());

    std::vector<LzdEncoderEntry> workspace(
        lzd_encoder_workspace_entries(input.size(), {}));
    const auto first = plan_lzd_token_stream(input, {}, {}, workspace);
    ASSERT_EQ(first.error, LzdEncodeError::none);
    const auto second = plan_lzd_token_stream(input, {}, {}, workspace);
    EXPECT_EQ(second.output_size, first.output_size);
    EXPECT_EQ(second.token_count, first.token_count);
    EXPECT_EQ(second.dictionary_entries, first.dictionary_entries);

    std::vector<std::byte> encoded(first.output_size);
    ASSERT_EQ(encode_lzd_token_stream(
                  input, {}, {}, workspace, encoded).error,
              LzdEncodeError::none);
    std::vector<std::byte> encoded_again(first.output_size);
    ASSERT_EQ(encode_lzd_token_stream(
                  input, {}, {}, workspace, encoded_again).error,
              LzdEncodeError::none);
    EXPECT_EQ(encoded_again, encoded);

    std::vector<LzdPhraseEntry> phrases(
        lzd_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::uint32_t> expansion(
        lzd_expansion_workspace_entries(phrases.size(), true));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzd_token_stream(
                  encoded, {}, input.size(), {}, phrases, expansion, decoded)
                  .error,
              LzdDecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(LzdEncoder, RoundTripsDeterministicPseudoRandomDataWithinTokenBound) {
    std::vector<std::byte> input(1025);
    std::uint32_t state = 0x4d415243;
    for (auto& value : input) {
        state = state * 1664525U + 1013904223U;
        value = static_cast<std::byte>(state >> 24);
    }

    const auto encoded = encode(input);
    EXPECT_LE(encoded.size(),
              ((input.size() + 1) / 2) * lzd_token_size);
    std::vector<LzdPhraseEntry> phrases(
        lzd_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::uint32_t> expansion(
        lzd_expansion_workspace_entries(phrases.size(), true));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzd_token_stream(
                  encoded, {}, input.size(), {}, phrases, expansion, decoded)
                  .error,
              LzdDecodeError::none);
    EXPECT_EQ(decoded, input);
    EXPECT_EQ(encode(input), encoded);
}

TEST(LzdEncoder, ExpectedFailuresAreAtomic) {
    const auto input = bytes("ABAB");
    std::array<LzdEncoderEntry, 2> workspace{};
    std::array<std::byte, 15> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzd_token_stream(
        input, {}, {}, workspace, output);
    EXPECT_EQ(result.error, LzdEncodeError::output_too_small);
    EXPECT_EQ(result.output_size, 16U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzd_token_stream(input, {}, {}, {}, output);
    EXPECT_EQ(result.error, LzdEncodeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 15;
    result = encode_lzd_token_stream(
        input, {}, limits, workspace, output);
    EXPECT_EQ(result.error, LzdEncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    LzdParameters parameters{};
    parameters.maximum_entries = 0;
    result = encode_lzd_token_stream(
        input, parameters, {}, workspace, output);
    EXPECT_EQ(result.error, LzdEncodeError::invalid_parameters);
    EXPECT_EQ(result.format_error,
              LzdFormatError::invalid_maximum_entries);
}

TEST(LzdEncoder, EnforcesInputAndAggregateWorkspaceLimits) {
    const auto input = bytes("ABAB");
    std::array<LzdEncoderEntry, 2> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = input.size() - 1;
    auto result = plan_lzd_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzdEncodeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        input.size() + 2 * sizeof(LzdEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = plan_lzd_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzdEncodeError::workspace_limit_exceeded);
}

} // namespace

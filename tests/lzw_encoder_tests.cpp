#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_encoder.hpp"

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
    const LzwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzwEncoderEntry> workspace(
        lzw_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lzw_code_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, LzwEncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lzw_code_stream(
                  input, parameters, limits, workspace, output).error,
              LzwEncodeError::none);
    return output;
}

std::vector<std::byte> encode(const std::string_view text) {
    return encode(bytes(text));
}

std::vector<std::byte> boundary_input() {
    std::vector<std::byte> input(2048);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<std::byte>(
            (index * 37 + index / 7) & 0xffU);
    }
    return input;
}

TEST(LzwEncoder, EmitsHandCheckableShortVectors) {
    EXPECT_TRUE(encode("").empty());

    constexpr std::array expected_a{
        std::byte{0x41}, std::byte{0x00}};
    EXPECT_EQ(encode("A"),
              std::vector<std::byte>(expected_a.begin(), expected_a.end()));

    constexpr std::array expected_aa{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    EXPECT_EQ(encode("AA"),
              std::vector<std::byte>(expected_aa.begin(), expected_aa.end()));

    constexpr std::array expected_aaa{
        std::byte{0x41}, std::byte{0x00}, std::byte{0x02}};
    EXPECT_EQ(encode("AAA"),
              std::vector<std::byte>(expected_aaa.begin(), expected_aaa.end()));

    constexpr std::array expected_abababa{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00},
        std::byte{0x14}, std::byte{0x08}};
    EXPECT_EQ(encode("ABABABA"), std::vector<std::byte>(
                                      expected_abababa.begin(),
                                      expected_abababa.end()));
}

TEST(LzwEncoder, EncodesEveryOneByteValueCanonically) {
    for (std::uint32_t value = 0; value < 256; ++value) {
        const std::array input{static_cast<std::byte>(value)};
        const auto encoded = encode(input);
        ASSERT_EQ(encoded.size(), 2U);
        EXPECT_EQ(encoded[0], input[0]);
        EXPECT_EQ(encoded[1], std::byte{});
    }
}

TEST(LzwEncoder, StoresInputBackedDictionaryPhrases) {
    const auto input = bytes("ABABABA");
    std::array<LzwEncoderEntry, 6> workspace{};
    const auto plan = plan_lzw_code_stream(input, {}, {}, workspace);
    ASSERT_EQ(plan.error, LzwEncodeError::none);
    EXPECT_EQ(plan.code_count, 4U);
    EXPECT_EQ(plan.dictionary_entries, 3U);
    EXPECT_EQ(plan.bit_count, 36U);
    EXPECT_EQ(plan.output_size, 5U);
    EXPECT_EQ(workspace[0].input_offset, 0U);
    EXPECT_EQ(workspace[0].length, 2U);
    EXPECT_EQ(workspace[1].input_offset, 1U);
    EXPECT_EQ(workspace[1].length, 2U);
    EXPECT_EQ(workspace[2].input_offset, 2U);
    EXPECT_EQ(workspace[2].length, 3U);
}

TEST(LzwEncoder, PlansDeterministicallyAcrossWidthBoundaryAndRoundTrips) {
    const auto input = boundary_input();
    std::vector<LzwEncoderEntry> encoder_workspace(
        lzw_encoder_workspace_entries(input.size(), {}));
    const auto first = plan_lzw_code_stream(
        input, {}, {}, encoder_workspace);
    ASSERT_EQ(first.error, LzwEncodeError::none);
    EXPECT_EQ(first.code_count, 969U);
    EXPECT_EQ(first.bit_count, 9635U);
    EXPECT_EQ(first.dictionary_entries, 968U);
    EXPECT_EQ(first.output_size, 1205U);
    const auto second = plan_lzw_code_stream(
        input, {}, {}, encoder_workspace);
    EXPECT_EQ(second.output_size, first.output_size);
    EXPECT_EQ(second.code_count, first.code_count);
    EXPECT_EQ(second.bit_count, first.bit_count);

    std::vector<std::byte> encoded(first.output_size);
    ASSERT_EQ(encode_lzw_code_stream(
                  input, {}, {}, encoder_workspace, encoded).error,
              LzwEncodeError::none);
    std::vector<std::byte> encoded_again(first.output_size);
    ASSERT_EQ(encode_lzw_code_stream(
                  input, {}, {}, encoder_workspace, encoded_again).error,
              LzwEncodeError::none);
    EXPECT_EQ(encoded_again, encoded);

    std::vector<LzwPhraseEntry> decoder_workspace(
        lzw_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzw_code_stream(
                  encoded, {}, input.size(), {}, decoder_workspace, decoded)
                  .error,
              LzwDecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(LzwEncoder, FreezesAtConfiguredMaximumAndRoundTrips) {
    const auto input = boundary_input();
    LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 256;
    std::vector<LzwEncoderEntry> encoder_workspace(
        lzw_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lzw_code_stream(
        input, parameters, limits, encoder_workspace);
    ASSERT_EQ(plan.error, LzwEncodeError::none);
    EXPECT_EQ(plan.code_count, 1255U);
    EXPECT_EQ(plan.bit_count, 11295U);
    EXPECT_EQ(plan.dictionary_entries, 256U);
    EXPECT_EQ(plan.output_size, 1412U);
    std::vector<std::byte> encoded(plan.output_size);
    ASSERT_EQ(encode_lzw_code_stream(
                  input, parameters, limits, encoder_workspace, encoded).error,
              LzwEncodeError::none);

    std::vector<LzwPhraseEntry> decoder_workspace(
        lzw_validation_workspace_entries(encoded.size(), parameters));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzw_code_stream(
                  encoded, parameters, input.size(), limits,
                  decoder_workspace, decoded).error,
              LzwDecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(LzwEncoder, ShortOutputAndPolicyFailuresAreAtomic) {
    const auto input = bytes("ABABABA");
    std::array<LzwEncoderEntry, 6> workspace{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzw_code_stream(
        input, {}, {}, workspace, output);
    EXPECT_EQ(result.error, LzwEncodeError::output_too_small);
    EXPECT_EQ(result.output_size, 5U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 4;
    result = encode_lzw_code_stream(input, {}, limits, workspace, output);
    EXPECT_EQ(result.error, LzwEncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzw_code_stream(input, {}, {}, {}, output);
    EXPECT_EQ(result.error, LzwEncodeError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    LzwParameters parameters{};
    parameters.maximum_code_width = 8;
    result = encode_lzw_code_stream(
        input, parameters, {}, workspace, output);
    EXPECT_EQ(result.error, LzwEncodeError::invalid_parameters);
    EXPECT_EQ(result.format_error, LzwFormatError::invalid_code_width);
}

TEST(LzwEncoder, EnforcesInputAndWorkspaceLimits) {
    const auto input = bytes("ABABABA");
    std::array<LzwEncoderEntry, 6> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = input.size() - 1;
    auto result = plan_lzw_code_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzwEncodeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        sizeof(LzwEncoderEntry) * (input.size() - 2);
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = plan_lzw_code_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, LzwEncodeError::workspace_limit_exceeded);
}

} // namespace

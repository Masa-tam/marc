#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"

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
    const Lz78Parameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<Lz78EncoderEntry> workspace(
        lz78_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lz78_token_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, Lz78EncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lz78_token_stream(
                  input, parameters, limits, workspace, output).error,
              Lz78EncodeError::none);
    return output;
}

std::vector<std::byte> encode(const std::string_view text) {
    return encode(bytes(text));
}

TEST(Lz78Encoder, EmitsHandCheckableEmptyAndSinglePair) {
    EXPECT_TRUE(encode("").empty());
    const auto encoded = encode("A");
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::equal(encoded.begin(), encoded.end(), expected.begin()));
}

TEST(Lz78Encoder, EncodesEveryOneByteValueCanonically) {
    for (unsigned int value = 0; value < 256; ++value) {
        const std::array input{static_cast<std::byte>(value)};
        const auto encoded = encode(input);
        ASSERT_EQ(encoded.size(), lz78_token_size);
        EXPECT_EQ(encoded[0], std::byte{0});
        EXPECT_EQ(encoded[1], input[0]);
        EXPECT_TRUE(std::all_of(
            encoded.begin() + 2, encoded.end(),
            [](const std::byte byte) { return byte == std::byte{}; }));
    }
}

TEST(Lz78Encoder, EmitsFinalExistingPhraseVectors) {
    const auto aa = encode("AA");
    constexpr std::array expected_aa{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(aa, std::vector<std::byte>(expected_aa.begin(), expected_aa.end()));

    const auto aba = encode("ABA");
    ASSERT_EQ(aba.size(), 3 * lz78_token_size);
    EXPECT_EQ(aba[16], std::byte{1});
    EXPECT_EQ(aba[20], std::byte{1});
}

TEST(Lz78Encoder, EmitsPairAtFrameEndAndNestedChain) {
    const auto abab = encode("ABAB");
    ASSERT_EQ(abab.size(), 3 * lz78_token_size);
    EXPECT_EQ(abab[16], std::byte{0});
    EXPECT_EQ(abab[17], std::byte{'B'});
    EXPECT_EQ(abab[20], std::byte{1});

    const auto nested = encode("AABABCABC");
    ASSERT_EQ(nested.size(), 4 * lz78_token_size);
    EXPECT_EQ(nested[8], std::byte{0});
    EXPECT_EQ(nested[12], std::byte{1});
    EXPECT_EQ(nested[16], std::byte{0});
    EXPECT_EQ(nested[20], std::byte{2});
    EXPECT_EQ(nested[24], std::byte{1});
    EXPECT_EQ(nested[28], std::byte{3});
}

TEST(Lz78Encoder, FreezesAtConfiguredMaximum) {
    const auto input = bytes("AAA");
    Lz78Parameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    const auto encoded = encode(input, parameters, limits);
    ASSERT_EQ(encoded.size(), 2 * lz78_token_size);
    EXPECT_EQ(encoded[8], std::byte{0});
    EXPECT_EQ(encoded[9], std::byte{'A'});
    EXPECT_EQ(encoded[12], std::byte{1});
}

TEST(Lz78Encoder, PlansExactlyAndRoundTripsBinaryData) {
    std::vector<std::byte> input;
    for (unsigned int value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());

    std::vector<Lz78EncoderEntry> encoder_workspace(
        lz78_encoder_workspace_entries(input.size(), {}));
    const auto first_plan = plan_lz78_token_stream(
        input, {}, {}, encoder_workspace);
    ASSERT_EQ(first_plan.error, Lz78EncodeError::none);
    const auto second_plan = plan_lz78_token_stream(
        input, {}, {}, encoder_workspace);
    EXPECT_EQ(second_plan.output_size, first_plan.output_size);
    EXPECT_EQ(second_plan.token_count, first_plan.token_count);

    std::vector<std::byte> encoded(first_plan.output_size);
    const auto encoded_result = encode_lz78_token_stream(
        input, {}, {}, encoder_workspace, encoded);
    ASSERT_EQ(encoded_result.error, Lz78EncodeError::none);
    EXPECT_EQ(encoded_result.output_size, first_plan.output_size);
    std::vector<std::byte> encoded_again(first_plan.output_size);
    ASSERT_EQ(encode_lz78_token_stream(
                  input, {}, {}, encoder_workspace, encoded_again).error,
              Lz78EncodeError::none);
    EXPECT_EQ(encoded_again, encoded);

    std::vector<Lz78PhraseEntry> decoder_workspace(
        lz78_validation_workspace_entries(encoded.size(), {}));
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lz78_token_stream(
                  encoded, {}, input.size(), {}, decoder_workspace, decoded)
                  .error,
              Lz78DecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(Lz78Encoder, ShortOutputAndPolicyFailuresAreAtomic) {
    const auto input = bytes("ABAB");
    std::array<Lz78EncoderEntry, 4> workspace{};
    std::array<std::byte, 23> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lz78_token_stream(
        input, {}, {}, workspace, output);
    EXPECT_EQ(result.error, Lz78EncodeError::output_too_small);
    EXPECT_EQ(result.output_size, 24U);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 23;
    result = encode_lz78_token_stream(input, {}, limits, workspace, output);
    EXPECT_EQ(result.error, Lz78EncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lz78_token_stream(input, {}, {}, {}, output);
    EXPECT_EQ(result.error, Lz78EncodeError::workspace_too_small);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    Lz78Parameters parameters{};
    parameters.maximum_entries = 0;
    result = encode_lz78_token_stream(
        input, parameters, {}, workspace, output);
    EXPECT_EQ(result.error, Lz78EncodeError::invalid_parameters);
    EXPECT_EQ(result.format_error, Lz78FormatError::invalid_maximum_entries);
}

TEST(Lz78Encoder, EnforcesInputAndWorkspaceLimits) {
    const auto input = bytes("ABAB");
    std::array<Lz78EncoderEntry, 4> workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = input.size() - 1;
    auto result = plan_lz78_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, Lz78EncodeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = sizeof(Lz78EncoderEntry) * 3;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = plan_lz78_token_stream(input, {}, limits, workspace);
    EXPECT_EQ(result.error, Lz78EncodeError::workspace_limit_exceeded);
}

} // namespace

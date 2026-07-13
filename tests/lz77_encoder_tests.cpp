#include "dictionary/lz77_decoder.hpp"
#include "dictionary/lz77_encoder.hpp"

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

std::vector<std::byte> encode(const std::string_view text) {
    const auto input = bytes(text);
    const auto plan = plan_lz77_token_stream(input, {}, {});
    EXPECT_EQ(plan.error, Lz77EncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lz77_token_stream(input, {}, {}, output).error,
              Lz77EncodeError::none);
    return output;
}

TEST(Lz77Encoder, EmitsHandCheckableEmptyAndSingleLiteral) {
    EXPECT_TRUE(encode("").empty());
    const auto encoded = encode("A");
    ASSERT_EQ(encoded.size(), lz77_token_size);
    EXPECT_EQ(encoded[0], std::byte{0});
    EXPECT_EQ(encoded[12], std::byte{'A'});
}

TEST(Lz77Encoder, EmitsHandCheckableOverlappingTerminalMatch) {
    const auto encoded = encode("AAAA");
    ASSERT_EQ(encoded.size(), 2 * lz77_token_size);
    EXPECT_EQ(encoded[0], std::byte{0});
    EXPECT_EQ(encoded[16], std::byte{2});
    EXPECT_EQ(encoded[20], std::byte{1});
    EXPECT_EQ(encoded[24], std::byte{3});
}

TEST(Lz77Encoder, EmitsHandCheckableDistanceTwoTerminalMatch) {
    const auto encoded = encode("ABABA");
    ASSERT_EQ(encoded.size(), 3 * lz77_token_size);
    EXPECT_EQ(encoded[32], std::byte{2});
    EXPECT_EQ(encoded[36], std::byte{2});
    EXPECT_EQ(encoded[40], std::byte{3});
}

TEST(Lz77Encoder, EmitsHandCheckableMatchThenLiteral) {
    const auto encoded = encode("ABCABCX");
    ASSERT_EQ(encoded.size(), 4 * lz77_token_size);
    EXPECT_EQ(encoded[48], std::byte{1});
    EXPECT_EQ(encoded[52], std::byte{3});
    EXPECT_EQ(encoded[56], std::byte{3});
    EXPECT_EQ(encoded[60], std::byte{'X'});
}

TEST(Lz77Encoder, PlansExactlyAndRoundTripsBinaryData) {
    std::vector<std::byte> input;
    for (unsigned int value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());
    const auto plan = plan_lz77_token_stream(input, {}, {});
    ASSERT_EQ(plan.error, Lz77EncodeError::none);
    std::vector<std::byte> encoded(plan.output_size);
    const auto encoded_result =
        encode_lz77_token_stream(input, {}, {}, encoded);
    ASSERT_EQ(encoded_result.error, Lz77EncodeError::none);
    EXPECT_EQ(encoded_result.output_size, plan.output_size);
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lz77_token_stream(encoded, {}, input.size(), {}, decoded)
                  .error,
              Lz77DecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(Lz77Encoder, ShortOutputAndPolicyFailuresAreAtomic) {
    const auto input = bytes("ABCABCX");
    std::array<std::byte, 63> output{};
    output.fill(std::byte{0xCC});
    auto result = encode_lz77_token_stream(input, {}, {}, output);
    EXPECT_EQ(result.error, Lz77EncodeError::output_too_small);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(),
                            [](std::byte value) { return value == std::byte{0xCC}; }));

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 63;
    result = encode_lz77_token_stream(input, {}, limits, output);
    EXPECT_EQ(result.error, Lz77EncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(),
                            [](std::byte value) { return value == std::byte{0xCC}; }));
}

TEST(Lz77Encoder, RespectsConfiguredMatchRange) {
    const auto input = bytes("AAAAA");
    Lz77Parameters parameters{};
    parameters.max_match_length = 3;
    const auto plan = plan_lz77_token_stream(input, parameters, {});
    ASSERT_EQ(plan.error, Lz77EncodeError::none);
    std::vector<std::byte> encoded(plan.output_size);
    ASSERT_EQ(encode_lz77_token_stream(input, parameters, {}, encoded).error,
              Lz77EncodeError::none);
    ASSERT_EQ(encoded.size(), 2 * lz77_token_size);
    EXPECT_EQ(encoded[16], std::byte{1});
    EXPECT_EQ(encoded[24], std::byte{3});
    EXPECT_EQ(encoded[28], std::byte{'A'});
}

} // namespace

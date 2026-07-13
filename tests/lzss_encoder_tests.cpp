#include "dictionary/lzss_decoder.hpp"
#include "dictionary/lzss_encoder.hpp"

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
    const auto plan = plan_lzss_token_stream(input, {}, {});
    EXPECT_EQ(plan.error, LzssEncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lzss_token_stream(input, {}, {}, output).error,
              LzssEncodeError::none);
    return output;
}

TEST(LzssEncoder, EmitsHandCheckableEmptyAndSingleLiteral) {
    EXPECT_TRUE(encode("").empty());
    const auto encoded = encode("A");
    constexpr std::array expected{std::byte{0x00}, std::byte{'A'}};
    ASSERT_EQ(encoded.size(), expected.size());
    EXPECT_TRUE(std::equal(encoded.begin(), encoded.end(), expected.begin()));
}

TEST(LzssEncoder, AppliesStrictCostBoundary) {
    const auto four_byte_candidate = encode("AAAAA");
    ASSERT_EQ(four_byte_candidate.size(), 10U);
    for (std::size_t offset = 0; offset < four_byte_candidate.size(); offset += 2)
        EXPECT_EQ(four_byte_candidate[offset], std::byte{0});

    const auto five_byte_candidate = encode("AAAAAA");
    constexpr std::array expected{
        std::byte{0x00}, std::byte{'A'},
        std::byte{0x01}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x05},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    ASSERT_EQ(five_byte_candidate.size(), expected.size());
    EXPECT_TRUE(std::equal(five_byte_candidate.begin(),
                          five_byte_candidate.end(), expected.begin()));
}

TEST(LzssEncoder, EmitsFrameEndingAndFollowedMatchVectors) {
    const auto ending = encode("ABCABCABC");
    ASSERT_EQ(ending.size(), 15U);
    EXPECT_EQ(ending[6], std::byte{0x01});
    EXPECT_EQ(ending[7], std::byte{0x03});
    EXPECT_EQ(ending[11], std::byte{0x06});

    const auto followed = encode("ABCABCABCX");
    ASSERT_EQ(followed.size(), 17U);
    EXPECT_TRUE(std::equal(ending.begin(), ending.end(), followed.begin()));
    EXPECT_EQ(followed[15], std::byte{0x00});
    EXPECT_EQ(followed[16], std::byte{'X'});
}

TEST(LzssEncoder, UsesNearestDistanceForEqualLength) {
    const auto encoded = encode("ABCDE1ABCDE2ABCDE3");
    std::size_t offset{};
    LzssToken last_match{};
    while (offset < encoded.size()) {
        LzssToken token{};
        std::size_t consumed{};
        ASSERT_EQ(parse_lzss_token(
                      std::span<const std::byte>{encoded}.subspan(offset),
                      token, consumed),
                  LzssFormatError::none);
        if (token.tag == LzssTokenTag::match) last_match = token;
        offset += consumed;
    }
    EXPECT_EQ(last_match.distance, 6U);
    EXPECT_EQ(last_match.length, 5U);
}

TEST(LzssEncoder, PlansExactlyAndRoundTripsBinaryData) {
    std::vector<std::byte> input;
    for (unsigned int value = 0; value < 256; ++value)
        input.push_back(static_cast<std::byte>(value));
    input.insert(input.end(), input.begin(), input.end());
    const auto plan = plan_lzss_token_stream(input, {}, {});
    ASSERT_EQ(plan.error, LzssEncodeError::none);
    std::vector<std::byte> encoded(plan.output_size);
    const auto encoded_result =
        encode_lzss_token_stream(input, {}, {}, encoded);
    ASSERT_EQ(encoded_result.error, LzssEncodeError::none);
    EXPECT_EQ(encoded_result.output_size, plan.output_size);
    std::vector<std::byte> decoded(input.size());
    ASSERT_EQ(decode_lzss_token_stream(encoded, {}, input.size(), {}, decoded)
                  .error,
              LzssDecodeError::none);
    EXPECT_EQ(decoded, input);
}

TEST(LzssEncoder, ShortOutputAndPolicyFailuresAreAtomic) {
    const auto input = bytes("ABCABCABCX");
    std::array<std::byte, 16> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzss_token_stream(input, {}, {}, output);
    EXPECT_EQ(result.error, LzssEncodeError::output_too_small);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 16;
    result = encode_lzss_token_stream(input, {}, limits, output);
    EXPECT_EQ(result.error, LzssEncodeError::serialized_limit_exceeded);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    result = encode_lzss_token_stream(input, parameters, {}, output);
    EXPECT_EQ(result.error, LzssEncodeError::invalid_parameters);
    EXPECT_EQ(result.format_error, LzssFormatError::invalid_match_range);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));

    limits = {};
    limits.max_frame_size = input.size() - 1;
    result = encode_lzss_token_stream(input, {}, limits, output);
    EXPECT_EQ(result.error, LzssEncodeError::input_limit_exceeded);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssEncoder, RespectsConfiguredMatchRange) {
    const auto input = bytes("AAAAAAAA");
    LzssParameters parameters{};
    parameters.max_match_length = 5;
    const auto plan = plan_lzss_token_stream(input, parameters, {});
    ASSERT_EQ(plan.error, LzssEncodeError::none);
    std::vector<std::byte> encoded(plan.output_size);
    ASSERT_EQ(encode_lzss_token_stream(input, parameters, {}, encoded).error,
              LzssEncodeError::none);
    ASSERT_EQ(encoded.size(), 15U);
    EXPECT_EQ(encoded[2], std::byte{0x01});
    EXPECT_EQ(encoded[7], std::byte{0x05});
    EXPECT_EQ(encoded[11], std::byte{0x00});
    EXPECT_EQ(encoded[13], std::byte{0x00});
}

} // namespace

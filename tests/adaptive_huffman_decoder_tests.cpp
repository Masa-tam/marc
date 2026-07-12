#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::AdaptiveHuffmanDecodeError;
using marc::entropy::internal::AdaptiveHuffmanDescriptor;

void expect_decode(const std::span<const std::byte> payload,
                   const AdaptiveHuffmanDescriptor descriptor,
                   const std::span<const std::byte> expected) {
    std::vector<std::byte> output(expected.size(), std::byte{0x5a});
    const auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        descriptor, payload, {}, output);
    ASSERT_EQ(result.error, AdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(result.output_size, expected.size());
    EXPECT_TRUE(std::ranges::equal(output, expected));
}

TEST(AdaptiveHuffmanDecoder, DecodesHandCheckableA) {
    constexpr std::array payload{std::byte{0x41}};
    constexpr std::array expected{std::byte{0x41}};
    expect_decode(payload, {1, 1, 8, 0}, expected);
}

TEST(AdaptiveHuffmanDecoder, DecodesHandCheckableAa) {
    constexpr std::array payload{std::byte{0x41}, std::byte{0x01}};
    constexpr std::array expected{std::byte{0x41}, std::byte{0x41}};
    expect_decode(payload, {2, 2, 1, 0}, expected);
}

TEST(AdaptiveHuffmanDecoder, DecodesHandCheckableAb) {
    constexpr std::array payload{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00}};
    constexpr std::array expected{std::byte{0x41}, std::byte{0x42}};
    expect_decode(payload, {2, 3, 1, 0}, expected);
}

TEST(AdaptiveHuffmanDecoder, DecodesHandCheckableAba) {
    constexpr std::array payload{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x02}};
    constexpr std::array expected{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    expect_decode(payload, {3, 3, 2, 0}, expected);
}

TEST(AdaptiveHuffmanDecoder, RoundTripsAllByteValues) {
    std::vector<std::byte> input;
    for (std::size_t repeat = 0; repeat < 4; ++repeat) {
        for (std::size_t symbol = 0; symbol < 256; ++symbol) {
            input.push_back(static_cast<std::byte>(symbol));
        }
    }
    AdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        input, {}, descriptor);
    ASSERT_EQ(plan.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> output(input.size());
    const auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        descriptor, payload, {}, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(output, input);
}

TEST(AdaptiveHuffmanDecoder, RejectsDuplicateNytWithoutOutput) {
    constexpr std::array payload{
        std::byte{0x41}, std::byte{0x82}, std::byte{0x00}};
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {2, 3, 1, 0}, payload, {}, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::duplicate_nyt_symbol);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(AdaptiveHuffmanDecoder, RejectsTruncationWithoutOutput) {
    constexpr std::array payload{std::byte{0x41}, std::byte{0x84}};
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {2, 2, 8, 0}, payload, {}, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::truncated_payload);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(AdaptiveHuffmanDecoder, RejectsTrailingBitsAndNonzeroPadding) {
    constexpr std::array trailing{std::byte{0x41}, std::byte{0x01}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {1, 2, 1, 0}, trailing, {}, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::trailing_bits);
    EXPECT_EQ(output[0], std::byte{0x5a});

    constexpr std::array bad_padding{
        std::byte{0x41}, std::byte{0x81}};
    std::array<std::byte, 2> padding_output{};
    result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {2, 2, 1, 0}, bad_padding, {}, padding_output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::nonzero_padding);
}

TEST(AdaptiveHuffmanDecoder, ChecksSizesBeforeOutput) {
    constexpr std::array payload{std::byte{0x41}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {1, 2, 8, 0}, payload, {}, output);
    EXPECT_EQ(result.error,
              AdaptiveHuffmanDecodeError::payload_size_mismatch);
    result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {2, 1, 8, 0}, payload, {}, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::output_too_small);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(AdaptiveHuffmanDecoder, EnforcesExpansionPolicyBeforeOutput) {
    constexpr std::array payload{std::byte{0x41}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    marc::core::DecoderLimits limits{};
    limits.max_expansion_ratio = 1;
    limits.expansion_slack = 0;
    const auto result = marc::entropy::internal::decode_adaptive_huffman_frame(
        {2, 1, 8, 0}, payload, limits, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanDecodeError::invalid_descriptor);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

} // namespace

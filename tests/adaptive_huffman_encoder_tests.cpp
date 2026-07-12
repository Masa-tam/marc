#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::AdaptiveHuffmanDescriptor;
using marc::entropy::internal::AdaptiveHuffmanEncodeError;

void expect_vector(const std::span<const std::byte> input,
                   const std::span<const std::byte> expected,
                   const std::uint8_t final_bits) {
    std::vector<std::byte> output(expected.size());
    AdaptiveHuffmanDescriptor descriptor{};
    const auto result = marc::entropy::internal::encode_adaptive_huffman_frame(
        input, {}, output, descriptor);
    ASSERT_EQ(result.error, AdaptiveHuffmanEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(output, expected));
    EXPECT_EQ(result.payload_size, expected.size());
    EXPECT_EQ(descriptor.symbol_count, input.size());
    EXPECT_EQ(descriptor.final_valid_bits, final_bits);
}

TEST(AdaptiveHuffmanEncoder, EmitsHandCheckableA) {
    constexpr std::array input{std::byte{0x41}};
    constexpr std::array expected{std::byte{0x41}};
    expect_vector(input, expected, 8);
}

TEST(AdaptiveHuffmanEncoder, EmitsHandCheckableAa) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x41}};
    constexpr std::array expected{std::byte{0x41}, std::byte{0x01}};
    expect_vector(input, expected, 1);
}

TEST(AdaptiveHuffmanEncoder, EmitsHandCheckableAb) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    constexpr std::array expected{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x00}};
    expect_vector(input, expected, 1);
}

TEST(AdaptiveHuffmanEncoder, EmitsHandCheckableAba) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    constexpr std::array expected{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x02}};
    expect_vector(input, expected, 2);
}

TEST(AdaptiveHuffmanEncoder, PlanningMatchesEncoding) {
    std::vector<std::byte> input;
    for (std::size_t repeat = 0; repeat < 3; ++repeat) {
        for (std::size_t symbol = 0; symbol < 256; ++symbol) {
            input.push_back(static_cast<std::byte>(symbol));
        }
    }
    AdaptiveHuffmanDescriptor planned{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        input, {}, planned);
    ASSERT_EQ(plan.error, AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> output(plan.payload_size, std::byte{0x5a});
    AdaptiveHuffmanDescriptor encoded{};
    const auto result = marc::entropy::internal::encode_adaptive_huffman_frame(
        input, {}, output, encoded);
    EXPECT_EQ(result.error, AdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(result.payload_size, plan.payload_size);
    EXPECT_EQ(result.payload_bits, plan.payload_bits);
    EXPECT_EQ(encoded.symbol_count, planned.symbol_count);
    EXPECT_EQ(encoded.payload_size, planned.payload_size);
    EXPECT_EQ(encoded.final_valid_bits, planned.final_valid_bits);
}

TEST(AdaptiveHuffmanEncoder, RejectsEmptyInputWithoutMutation) {
    std::array<std::byte, 1> output{std::byte{0x5a}};
    AdaptiveHuffmanDescriptor descriptor{7, 7, 7, 7};
    const auto result = marc::entropy::internal::encode_adaptive_huffman_frame(
        {}, {}, output, descriptor);
    EXPECT_EQ(result.error, AdaptiveHuffmanEncodeError::empty_input);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(AdaptiveHuffmanEncoder, ReportsCapacityBeforeWritingOutput) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0x5a});
    AdaptiveHuffmanDescriptor descriptor{7, 7, 7, 7};
    const auto result = marc::entropy::internal::encode_adaptive_huffman_frame(
        input, {}, output, descriptor);
    EXPECT_EQ(result.error,
              AdaptiveHuffmanEncodeError::payload_output_too_small);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(AdaptiveHuffmanEncoder, EnforcesLocalPayloadLimit) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 2;
    AdaptiveHuffmanDescriptor descriptor{};
    const auto result = marc::entropy::internal::plan_adaptive_huffman_frame(
        input, limits, descriptor);
    EXPECT_EQ(result.error, AdaptiveHuffmanEncodeError::limit_exceeded);
    EXPECT_EQ(result.payload_size, 3U);
}

} // namespace

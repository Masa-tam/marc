#include "entropy/adaptive_huffman_format.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::entropy::internal::AdaptiveHuffmanDescriptor;
using marc::entropy::internal::AdaptiveHuffmanFormatError;

TEST(AdaptiveHuffmanFormat, SerializesHandCheckableAVector) {
    const AdaptiveHuffmanDescriptor descriptor{1, 1, 8, 0};
    std::array<std::byte, 16> output{};
    ASSERT_EQ(marc::entropy::internal::serialize_adaptive_huffman_descriptor(
                  descriptor, 1, 1, {}, output),
              AdaptiveHuffmanFormatError::none);
    const std::array expected{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_EQ(output, expected);
}

TEST(AdaptiveHuffmanFormat, ParsesHandCheckableAbaVector) {
    const std::array input{
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    AdaptiveHuffmanDescriptor descriptor{};
    ASSERT_EQ(marc::entropy::internal::parse_adaptive_huffman_descriptor(
                  input, 3, 3, {}, descriptor),
              AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(descriptor.symbol_count, 3U);
    EXPECT_EQ(descriptor.payload_size, 3U);
    EXPECT_EQ(descriptor.final_valid_bits, 2U);
}

TEST(AdaptiveHuffmanFormat, RejectsContradictionsWithoutPublishingOutput) {
    std::array<std::byte, 16> input{};
    input[0] = std::byte{1};
    input[4] = std::byte{1};
    input[8] = std::byte{8};
    AdaptiveHuffmanDescriptor descriptor{9, 9, 9, 9};
    EXPECT_EQ(marc::entropy::internal::parse_adaptive_huffman_descriptor(
                  input, 2, 1, {}, descriptor),
              AdaptiveHuffmanFormatError::contradictory_size);
    EXPECT_EQ(descriptor.symbol_count, 9U);
}

TEST(AdaptiveHuffmanFormat, RejectsInvalidBitsFlagsAndReservedBytes) {
    AdaptiveHuffmanDescriptor descriptor{1, 1, 0, 0};
    EXPECT_EQ(marc::entropy::internal::validate_adaptive_huffman_descriptor(
                  descriptor, 1, 1, {}),
              AdaptiveHuffmanFormatError::invalid_final_bits);
    descriptor.final_valid_bits = 8;
    descriptor.flags = 1;
    EXPECT_EQ(marc::entropy::internal::validate_adaptive_huffman_descriptor(
                  descriptor, 1, 1, {}),
              AdaptiveHuffmanFormatError::unknown_flags);
    std::array<std::byte, 16> input{};
    input[0] = std::byte{1};
    input[4] = std::byte{1};
    input[8] = std::byte{8};
    input[15] = std::byte{1};
    AdaptiveHuffmanDescriptor parsed{};
    EXPECT_EQ(marc::entropy::internal::parse_adaptive_huffman_descriptor(
                  input, 1, 1, {}, parsed),
              AdaptiveHuffmanFormatError::nonzero_reserved);
}

TEST(AdaptiveHuffmanFormat, EnforcesVariantAndLocalFrameLimits) {
    AdaptiveHuffmanDescriptor descriptor{
        marc::entropy::internal::adaptive_huffman_max_frame_size + 1U,
        1, 8, 0};
    EXPECT_EQ(marc::entropy::internal::validate_adaptive_huffman_descriptor(
                  descriptor, descriptor.symbol_count, 1, {}),
              AdaptiveHuffmanFormatError::invalid_symbol_count);
    descriptor = {2, 1, 8, 0};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 1;
    EXPECT_EQ(marc::entropy::internal::validate_adaptive_huffman_descriptor(
                  descriptor, 2, 1, limits),
              AdaptiveHuffmanFormatError::limit_exceeded);
}

} // namespace

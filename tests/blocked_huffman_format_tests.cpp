#include "entropy/blocked_huffman_format.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::BlockedHuffmanDescriptor;
using marc::entropy::internal::BlockedHuffmanFormatError;

[[nodiscard]] BlockedHuffmanDescriptor raw_descriptor(
    const std::uint32_t size) {
    return {size, size, 0,
            marc::entropy::internal::blocked_huffman_raw_flag, 8};
}

[[nodiscard]] BlockedHuffmanDescriptor huffman_descriptor() {
    return {1024, 128,
            marc::entropy::internal::blocked_huffman_model_size, 0, 8};
}

} // namespace

TEST(BlockedHuffmanDescriptorTest, SerializesHandCheckableRawVector) {
    constexpr std::array expected{
        std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x08},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::array<std::byte,
               marc::entropy::internal::blocked_huffman_descriptor_size>
        output{};
    ASSERT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  raw_descriptor(4), 4, marc::core::DecoderLimits{}, output),
              BlockedHuffmanFormatError::none);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanDescriptorTest, ParsesHandCheckableRawVector) {
    constexpr std::array input{
        std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x01}, std::byte{0x08},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    BlockedHuffmanDescriptor descriptor{};
    ASSERT_EQ(marc::entropy::internal::parse_block_descriptor(
                  input, 4, marc::core::DecoderLimits{}, descriptor),
              BlockedHuffmanFormatError::none);
    EXPECT_EQ(descriptor.symbol_count, 4U);
    EXPECT_EQ(descriptor.payload_size, 4U);
    EXPECT_EQ(descriptor.final_valid_bits, 8);
}

TEST(BlockedHuffmanDescriptorTest, AcceptsSmallerHuffmanRepresentation) {
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  huffman_descriptor(), 1024, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::none);
}

TEST(BlockedHuffmanDescriptorTest, AcceptsPartialFinalHuffmanByte) {
    auto descriptor = huffman_descriptor();
    descriptor.payload_size = 129;
    descriptor.final_valid_bits = 1;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 1024, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::none);
}

TEST(BlockedHuffmanDescriptorTest, RejectsContradictoryRawFields) {
    auto descriptor = raw_descriptor(4);
    descriptor.payload_size = 3;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 4, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::contradictory_representation);
    descriptor = raw_descriptor(4);
    descriptor.final_valid_bits = 7;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 4, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::invalid_final_bits);
}

TEST(BlockedHuffmanDescriptorTest, RejectsHuffmanThatCannotBeatRaw) {
    auto descriptor = huffman_descriptor();
    descriptor.symbol_count = 384;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 384, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::contradictory_representation);
}

TEST(BlockedHuffmanDescriptorTest, RejectsImpossiblePayloadBitCount) {
    auto descriptor = huffman_descriptor();
    descriptor.payload_size = 1;
    descriptor.final_valid_bits = 8;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 1024, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::contradictory_representation);
}

TEST(BlockedHuffmanDescriptorTest, RejectsUnknownFlagsAndReservedBytes) {
    auto descriptor = raw_descriptor(4);
    descriptor.flags = 0x81;
    EXPECT_EQ(marc::entropy::internal::validate_block_descriptor(
                  descriptor, 4, marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::unknown_flags);

    std::array<std::byte,
               marc::entropy::internal::blocked_huffman_descriptor_size>
        input{};
    ASSERT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  raw_descriptor(4), 4, marc::core::DecoderLimits{}, input),
              BlockedHuffmanFormatError::none);
    input[15] = std::byte{1};
    BlockedHuffmanDescriptor parsed = raw_descriptor(9);
    EXPECT_EQ(marc::entropy::internal::parse_block_descriptor(
                  input, 4, marc::core::DecoderLimits{}, parsed),
              BlockedHuffmanFormatError::nonzero_reserved);
    EXPECT_EQ(parsed.symbol_count, 9U);
}

TEST(BlockedHuffmanLayoutTest, AcceptsFinalShortRawBlock) {
    const std::array descriptors{raw_descriptor(4), raw_descriptor(2)};
    EXPECT_EQ(marc::entropy::internal::validate_blocked_huffman_layout(
                  descriptors, 6, 4, 32, 6,
                  marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::none);
}

TEST(BlockedHuffmanLayoutTest, RequiresExactBlockCountAndSizes) {
    const std::array descriptors{raw_descriptor(4), raw_descriptor(2)};
    EXPECT_EQ(marc::entropy::internal::validate_blocked_huffman_layout(
                  std::span{descriptors}.first(1), 6, 4, 16, 4,
                  marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::invalid_block_count);
    EXPECT_EQ(marc::entropy::internal::validate_blocked_huffman_layout(
                  descriptors, 6, 4, 31, 6,
                  marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::descriptor_size_mismatch);
    EXPECT_EQ(marc::entropy::internal::validate_blocked_huffman_layout(
                  descriptors, 6, 4, 32, 7,
                  marc::core::DecoderLimits{}),
              BlockedHuffmanFormatError::payload_size_mismatch);
}

TEST(BlockedHuffmanLayoutTest, EnforcesCombinedBufferedLimit) {
    const std::array descriptors{raw_descriptor(4), raw_descriptor(2)};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 37;
    EXPECT_EQ(marc::entropy::internal::validate_blocked_huffman_layout(
                  descriptors, 6, 4, 32, 6, limits),
              BlockedHuffmanFormatError::limit_exceeded);
}

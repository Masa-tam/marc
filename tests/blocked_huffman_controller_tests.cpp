#include "entropy/blocked_huffman_controller.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::BlockedHuffmanBlockView;
using marc::entropy::internal::BlockedHuffmanControllerError;
using marc::entropy::internal::BlockedHuffmanDescriptor;
using marc::entropy::internal::BlockedHuffmanFormatError;

[[nodiscard]] std::array<std::byte, 32> two_raw_descriptors() {
    std::array<std::byte, 32> region{};
    const BlockedHuffmanDescriptor first{4, 4, 0, 1, 8};
    const BlockedHuffmanDescriptor second{2, 2, 0, 1, 8};
    std::span<std::byte, 16> first_output{region.data(), 16};
    std::span<std::byte, 16> second_output{region.data() + 16, 16};
    EXPECT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  first, 4, marc::core::DecoderLimits{}, first_output),
              BlockedHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  second, 2, marc::core::DecoderLimits{}, second_output),
              BlockedHuffmanFormatError::none);
    return region;
}

} // namespace

TEST(BlockedHuffmanControllerTest, ParsesTwoRawBlocksAndOffsets) {
    const auto region = two_raw_descriptors();
    std::array<BlockedHuffmanBlockView, 2> views{};
    const auto result =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            region, 6, 4, 2, 6, marc::core::DecoderLimits{}, views);
    ASSERT_EQ(result.error, BlockedHuffmanControllerError::none);
    EXPECT_EQ(result.block_count, 2U);
    EXPECT_EQ(views[0].descriptor.symbol_count, 4U);
    EXPECT_EQ(views[0].model_offset, 16U);
    EXPECT_EQ(views[0].payload_offset, 0U);
    EXPECT_EQ(views[1].descriptor.symbol_count, 2U);
    EXPECT_EQ(views[1].model_offset, 32U);
    EXPECT_EQ(views[1].payload_offset, 4U);
}

TEST(BlockedHuffmanControllerTest, ReportsRequiredViewCapacity) {
    const auto region = two_raw_descriptors();
    std::array<BlockedHuffmanBlockView, 1> views{};
    const auto result =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            region, 6, 4, 2, 6, marc::core::DecoderLimits{}, views);
    EXPECT_EQ(result.error,
              BlockedHuffmanControllerError::output_views_too_small);
    EXPECT_EQ(result.block_count, 2U);
}

TEST(BlockedHuffmanControllerTest, RejectsTruncatedAndTrailingRegions) {
    const auto region = two_raw_descriptors();
    std::array<BlockedHuffmanBlockView, 2> views{};
    EXPECT_EQ(marc::entropy::internal::parse_blocked_huffman_descriptor_region(
                  std::span<const std::byte>{region}.first(31),
                  6, 4, 2, 6, marc::core::DecoderLimits{}, views).error,
              BlockedHuffmanControllerError::truncated_descriptor);

    std::array<std::byte, 33> extended{};
    std::ranges::copy(region, extended.begin());
    EXPECT_EQ(marc::entropy::internal::parse_blocked_huffman_descriptor_region(
                  extended, 6, 4, 2, 6,
                  marc::core::DecoderLimits{}, views).error,
              BlockedHuffmanControllerError::trailing_descriptor_bytes);
}

TEST(BlockedHuffmanControllerTest, RejectsPayloadTotalMismatch) {
    const auto region = two_raw_descriptors();
    std::array<BlockedHuffmanBlockView, 2> views{};
    EXPECT_EQ(marc::entropy::internal::parse_blocked_huffman_descriptor_region(
                  region, 6, 4, 2, 7,
                  marc::core::DecoderLimits{}, views).error,
              BlockedHuffmanControllerError::payload_size_mismatch);
}

TEST(BlockedHuffmanControllerTest, ValidatesModelBeforePublishingViews) {
    const BlockedHuffmanDescriptor descriptor{1024, 128, 256, 0, 8};
    std::array<std::byte, 272> region{};
    std::span<std::byte, 16> encoded{region.data(), 16};
    ASSERT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  descriptor, 1024, marc::core::DecoderLimits{}, encoded),
              BlockedHuffmanFormatError::none);
    region[16] = std::byte{1};
    region[17] = std::byte{1};
    region[18] = std::byte{1};

    std::array<BlockedHuffmanBlockView, 1> views{};
    views[0].model_offset = 77;
    const auto result =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            region, 1024, 1024, 1, 128,
            marc::core::DecoderLimits{}, views);
    EXPECT_EQ(result.error, BlockedHuffmanControllerError::invalid_model);
    EXPECT_EQ(views[0].model_offset, 77U);
}

TEST(BlockedHuffmanControllerTest, PublishesValidatedHuffmanModelOffset) {
    const BlockedHuffmanDescriptor descriptor{1024, 128, 256, 0, 8};
    std::array<std::byte, 272> region{};
    std::span<std::byte, 16> encoded{region.data(), 16};
    ASSERT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  descriptor, 1024, marc::core::DecoderLimits{}, encoded),
              BlockedHuffmanFormatError::none);
    region[16 + 0x41] = std::byte{1};

    std::array<BlockedHuffmanBlockView, 1> views{};
    const auto result =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            region, 1024, 1024, 1, 128,
            marc::core::DecoderLimits{}, views);
    ASSERT_EQ(result.error, BlockedHuffmanControllerError::none);
    EXPECT_EQ(views[0].model_offset, 16U);
    EXPECT_EQ(views[0].payload_offset, 0U);
    EXPECT_EQ(views[0].descriptor.model_size, 256U);
}

TEST(BlockedHuffmanControllerTest, RejectsContradictoryDeclaredBlockCount) {
    const auto region = two_raw_descriptors();
    std::array<BlockedHuffmanBlockView, 2> views{};
    const auto result =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            region, 6, 4, 1, 6, marc::core::DecoderLimits{}, views);
    EXPECT_EQ(result.error,
              BlockedHuffmanControllerError::invalid_block_count);
    EXPECT_EQ(result.block_count, 2U);
}

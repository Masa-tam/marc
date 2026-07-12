#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::entropy::internal::BlockedHuffmanBlockView;
using marc::entropy::internal::BlockedHuffmanControllerError;
using marc::entropy::internal::BlockedHuffmanFrameDecodeError;
using marc::entropy::internal::BlockedHuffmanFrameEncodeError;

} // namespace

TEST(BlockedHuffmanFrameEncoderTest, PlansTwoHuffmanBlocksExactly) {
    const std::vector<std::byte> input(600, std::byte{0x41});
    const auto plan =
        marc::entropy::internal::plan_blocked_huffman_frame(
            input, 300, marc::core::DecoderLimits{});
    EXPECT_EQ(plan.error, BlockedHuffmanFrameEncodeError::none);
    EXPECT_EQ(plan.block_count, 2U);
    EXPECT_EQ(plan.descriptor_region_size, 544U);
    EXPECT_EQ(plan.payload_size, 76U);
}

TEST(BlockedHuffmanFrameEncoderTest, EncodesAndDecodesMultipleBlocks) {
    std::vector<std::byte> input(300, std::byte{0x41});
    input.push_back(std::byte{1});
    input.push_back(std::byte{2});
    input.push_back(std::byte{3});
    input.push_back(std::byte{4});

    const auto plan =
        marc::entropy::internal::plan_blocked_huffman_frame(
            input, 300, marc::core::DecoderLimits{});
    ASSERT_EQ(plan.error, BlockedHuffmanFrameEncodeError::none);
    EXPECT_EQ(plan.block_count, 2U);
    EXPECT_EQ(plan.descriptor_region_size, 288U);
    EXPECT_EQ(plan.payload_size, 42U);

    std::vector<std::byte> descriptors(plan.descriptor_region_size);
    std::vector<std::byte> payload(plan.payload_size);
    const auto encoded =
        marc::entropy::internal::encode_blocked_huffman_frame(
            input, 300, marc::core::DecoderLimits{}, descriptors, payload);
    ASSERT_EQ(encoded.error, BlockedHuffmanFrameEncodeError::none);

    std::array<BlockedHuffmanBlockView, 2> views{};
    const auto parsed =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            descriptors, static_cast<std::uint32_t>(input.size()), 300, 2,
            static_cast<std::uint32_t>(payload.size()),
            marc::core::DecoderLimits{}, views);
    ASSERT_EQ(parsed.error, BlockedHuffmanControllerError::none);
    EXPECT_EQ(views[0].descriptor.model_size, 256U);
    EXPECT_EQ(views[1].descriptor.model_size, 0U);
    EXPECT_EQ(views[1].payload_offset, 38U);

    std::vector<std::byte> output(input.size());
    const auto decoded =
        marc::entropy::internal::decode_blocked_huffman_frame(
            descriptors, payload, views, marc::core::DecoderLimits{}, output);
    ASSERT_EQ(decoded.error, BlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(output, input);
}

TEST(BlockedHuffmanFrameEncoderTest, ReportsCapacityWithoutMutation) {
    const std::vector<std::byte> input(600, std::byte{0x41});
    std::array<std::byte, 543> descriptors{};
    std::array<std::byte, 76> payload{};
    descriptors.fill(std::byte{0x5a});
    payload.fill(std::byte{0x6b});
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_frame(
            input, 300, marc::core::DecoderLimits{}, descriptors, payload);
    EXPECT_EQ(result.error,
              BlockedHuffmanFrameEncodeError::descriptor_output_too_small);
    EXPECT_EQ(result.descriptor_region_size, 544U);
    EXPECT_EQ(result.payload_size, 76U);
    EXPECT_TRUE(std::ranges::all_of(
        descriptors,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_TRUE(std::ranges::all_of(
        payload,
        [](const std::byte value) { return value == std::byte{0x6b}; }));
}

TEST(BlockedHuffmanFrameEncoderTest, ChecksPayloadBeforeAnyOutput) {
    const std::vector<std::byte> input(600, std::byte{0x41});
    std::array<std::byte, 544> descriptors{};
    std::array<std::byte, 75> payload{};
    descriptors.fill(std::byte{0x5a});
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_frame(
            input, 300, marc::core::DecoderLimits{}, descriptors, payload);
    EXPECT_EQ(result.error,
              BlockedHuffmanFrameEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        descriptors,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanFrameEncoderTest, RejectsEmptyAndInvalidBlockSize) {
    EXPECT_EQ(marc::entropy::internal::plan_blocked_huffman_frame(
                  {}, 300, marc::core::DecoderLimits{}).error,
              BlockedHuffmanFrameEncodeError::empty_input);
    const std::array input{std::byte{1}};
    EXPECT_EQ(marc::entropy::internal::plan_blocked_huffman_frame(
                  input, 0, marc::core::DecoderLimits{}).error,
              BlockedHuffmanFrameEncodeError::invalid_block_size);
}

#include "entropy/blocked_huffman_encoder.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::entropy::internal::BlockedHuffmanBlockView;
using marc::entropy::internal::BlockedHuffmanControllerError;
using marc::entropy::internal::BlockedHuffmanDecodeError;
using marc::entropy::internal::BlockedHuffmanDescriptor;
using marc::entropy::internal::BlockedHuffmanEncodeError;
using marc::entropy::internal::BlockedHuffmanFormatError;
using marc::entropy::internal::BlockedHuffmanFrameDecodeError;

struct TwoBlockFrame {
    std::array<std::byte, 544> descriptors{};
    std::array<std::byte, 76> payload{};
    std::array<BlockedHuffmanBlockView, 2> views{};
};

[[nodiscard]] TwoBlockFrame make_two_huffman_blocks() {
    TwoBlockFrame frame{};
    const std::vector<std::byte> input(300, std::byte{0x41});
    std::array<std::byte, 256> model{};
    std::array<std::byte, 38> payload{};
    BlockedHuffmanDescriptor descriptor{};
    const auto encoded =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    EXPECT_EQ(encoded.error, BlockedHuffmanEncodeError::none);

    for (std::size_t block = 0; block < 2; ++block) {
        const auto descriptor_offset = block * 272;
        std::span<std::byte, 16> descriptor_output{
            frame.descriptors.data() + descriptor_offset, 16};
        EXPECT_EQ(marc::entropy::internal::serialize_block_descriptor(
                      descriptor, 300, marc::core::DecoderLimits{},
                      descriptor_output),
                  BlockedHuffmanFormatError::none);
        std::ranges::copy(
            model, frame.descriptors.begin() + descriptor_offset + 16);
        std::ranges::copy(
            payload, frame.payload.begin() + block * payload.size());
    }

    const auto parsed =
        marc::entropy::internal::parse_blocked_huffman_descriptor_region(
            frame.descriptors, 600, 300, 2, 76,
            marc::core::DecoderLimits{}, frame.views);
    EXPECT_EQ(parsed.error, BlockedHuffmanControllerError::none);
    return frame;
}

} // namespace

TEST(BlockedHuffmanFrameDecoderTest, DecodesAllBlocksInOrder) {
    const auto frame = make_two_huffman_blocks();
    std::array<std::byte, 600> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, frame.payload, frame.views,
            marc::core::DecoderLimits{}, output);
    ASSERT_EQ(result.error, BlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(result.output_size, 600U);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x41}; }));
}

TEST(BlockedHuffmanFrameDecoderTest, ValidatesLaterBlockBeforeAnyOutput) {
    auto frame = make_two_huffman_blocks();
    frame.payload[38] = std::byte{1};
    std::array<std::byte, 600> output{};
    output.fill(std::byte{0x5a});
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, frame.payload, frame.views,
            marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanFrameDecodeError::block_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.block_error, BlockedHuffmanDecodeError::invalid_code);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanFrameDecoderTest, ReportsWholeFrameOutputCapacity) {
    const auto frame = make_two_huffman_blocks();
    std::array<std::byte, 599> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, frame.payload, frame.views,
            marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error,
              BlockedHuffmanFrameDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 600U);
}

TEST(BlockedHuffmanFrameDecoderTest, RejectsDiscontinuousPayloadViews) {
    auto frame = make_two_huffman_blocks();
    frame.views[1].payload_offset = 37;
    std::array<std::byte, 600> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, frame.payload, frame.views,
            marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanFrameDecodeError::invalid_view);
    EXPECT_EQ(result.block_index, 1U);
}

TEST(BlockedHuffmanFrameDecoderTest, RejectsMutatedDescriptorInView) {
    auto frame = make_two_huffman_blocks();
    frame.views[1].descriptor.payload_size = 37;
    std::array<std::byte, 600> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, frame.payload, frame.views,
            marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanFrameDecodeError::block_error);
    EXPECT_EQ(result.block_index, 1U);
    EXPECT_EQ(result.block_error,
              BlockedHuffmanDecodeError::invalid_descriptor);
}

TEST(BlockedHuffmanFrameDecoderTest, RejectsUnreferencedPayloadTail) {
    const auto frame = make_two_huffman_blocks();
    std::array<std::byte, 77> extended_payload{};
    std::ranges::copy(frame.payload, extended_payload.begin());
    std::array<std::byte, 600> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_frame(
            frame.descriptors, extended_payload, frame.views,
            marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanFrameDecodeError::invalid_view);
}

#include "frame/blocked_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::frame::BlockedHuffmanStreamCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 304;
    stream.entropy_block_size = 300;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> two_frame_input() {
    std::vector<std::byte> input(300, std::byte{0x41});
    input.push_back(std::byte{1});
    input.push_back(std::byte{2});
    input.push_back(std::byte{3});
    input.push_back(std::byte{4});
    input.insert(input.end(), 300, std::byte{0x42});
    return input;
}

[[nodiscard]] std::vector<std::byte> encode_stream(
    const std::vector<std::byte>& input) {
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    EXPECT_EQ(plan.error, BlockedHuffmanStreamCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    const auto result = marc::frame::encode_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input, encoded);
    EXPECT_EQ(result.error, BlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(BlockedHuffmanStreamTest, RoundTripsMultipleFrames) {
    const auto input = two_frame_input();
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream_for(input.size()), marc::core::DecoderLimits{}, input);
    ASSERT_EQ(plan.error, BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 816U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encode_stream(input);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size());
    marc::frame::StreamHeader decoded_stream{};
    const auto result = marc::frame::decode_blocked_huffman_stream(
        encoded, marc::core::DecoderLimits{}, views, output, decoded_stream);
    ASSERT_EQ(result.error, BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(result.output_size, input.size());
    EXPECT_EQ(decoded_stream.original_size, input.size());
    EXPECT_EQ(output, input);
}

TEST(BlockedHuffmanStreamTest, EmptyInputIsHeaderOnly) {
    const std::vector<std::byte> input;
    auto stream = stream_for(0);
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    ASSERT_EQ(plan.error, BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, marc::frame::stream_header_size);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, marc::frame::stream_header_size> encoded{};
    ASSERT_EQ(marc::frame::encode_blocked_huffman_stream(
                  stream, marc::core::DecoderLimits{}, input, encoded).error,
              BlockedHuffmanStreamCodecError::none);
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_blocked_huffman_stream(
                  encoded, marc::core::DecoderLimits{}, {}, {}, decoded).error,
              BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(decoded.original_size, 0U);
}

TEST(BlockedHuffmanStreamTest, PlansBeforeWritingAnyOutput) {
    const auto input = two_frame_input();
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    ASSERT_EQ(plan.error, BlockedHuffmanStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size - 1, std::byte{0x5a});
    const auto result = marc::frame::encode_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input, output);
    EXPECT_EQ(result.error, BlockedHuffmanStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanStreamTest, LaterFrameCorruptionLeavesOutputUntouched) {
    const auto input = two_frame_input();
    auto encoded = encode_stream(input);
    constexpr std::size_t first_frame_size = 386;
    constexpr std::size_t second_payload_offset =
        marc::frame::stream_header_size + first_frame_size
        + marc::frame::frame_header_size + 272;
    encoded[second_payload_offset] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size(), std::byte{0x5a});
    marc::frame::StreamHeader decoded{};
    const auto result = marc::frame::decode_blocked_huffman_stream(
        encoded, marc::core::DecoderLimits{}, views, output, decoded);
    EXPECT_EQ(result.error, BlockedHuffmanStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanStreamTest, RejectsTruncationAndTrailingBytes) {
    const auto input = two_frame_input();
    const auto encoded = encode_stream(input);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size());
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_blocked_huffman_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  marc::core::DecoderLimits{}, views, output, decoded).error,
              BlockedHuffmanStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_blocked_huffman_stream(
                  extended, marc::core::DecoderLimits{},
                  views, output, decoded).error,
              BlockedHuffmanStreamCodecError::trailing_stream_bytes);
}

TEST(BlockedHuffmanStreamTest, RejectsSequenceMismatchInLaterFrame) {
    const auto input = two_frame_input();
    auto encoded = encode_stream(input);
    constexpr std::size_t second_header =
        marc::frame::stream_header_size + 386;
    encoded[second_header + 8] = std::byte{0};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size(), std::byte{0x5a});
    marc::frame::StreamHeader decoded{};
    const auto result = marc::frame::decode_blocked_huffman_stream(
        encoded, marc::core::DecoderLimits{}, views, output, decoded);
    EXPECT_EQ(result.error, BlockedHuffmanStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanStreamTest, ReportsScratchAndOutputCapacity) {
    const auto input = two_frame_input();
    const auto encoded = encode_stream(input);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::vector<std::byte> output(input.size());
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_blocked_huffman_stream(
                  encoded, marc::core::DecoderLimits{},
                  views, output, decoded).error,
              BlockedHuffmanStreamCodecError::view_output_too_small);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> full_views{};
    output.resize(input.size() - 1);
    EXPECT_EQ(marc::frame::decode_blocked_huffman_stream(
                  encoded, marc::core::DecoderLimits{},
                  full_views, output, decoded).error,
              BlockedHuffmanStreamCodecError::output_too_small);
}

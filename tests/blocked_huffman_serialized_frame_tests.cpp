#include "frame/blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::frame::BlockedHuffmanFrameCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = static_cast<std::uint32_t>(size);
    stream.entropy_block_size = 300;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> mixed_input() {
    std::vector<std::byte> input(300, std::byte{0x41});
    input.push_back(std::byte{1});
    input.push_back(std::byte{2});
    input.push_back(std::byte{3});
    input.push_back(std::byte{4});
    return input;
}

[[nodiscard]] std::vector<std::byte> encoded_mixed_frame() {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_blocked_huffman_frame(
        stream, marc::core::DecoderLimits{}, 0, 0, input);
    EXPECT_EQ(plan.error, BlockedHuffmanFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    const auto result = marc::frame::encode_blocked_huffman_frame(
        stream, marc::core::DecoderLimits{}, 0, 0, input, encoded);
    EXPECT_EQ(result.error, BlockedHuffmanFrameCodecError::none);
    return encoded;
}

} // namespace

TEST(BlockedHuffmanSerializedFrameTest, PlansExactSerializedSize) {
    const auto input = mixed_input();
    const auto result = marc::frame::plan_blocked_huffman_frame(
        stream_for(input.size()), marc::core::DecoderLimits{},
        0, 0, input);
    EXPECT_EQ(result.error, BlockedHuffmanFrameCodecError::none);
    EXPECT_EQ(result.serialized_size, 386U);
    EXPECT_EQ(result.output_size, input.size());
    EXPECT_EQ(result.block_count, 2U);
}

TEST(BlockedHuffmanSerializedFrameTest, EncodesCanonicalFrameHeader) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    const auto encoded = encoded_mixed_frame();
    const std::span<const std::byte, marc::frame::frame_header_size> header_bytes{
        encoded.data(), marc::frame::frame_header_size};
    marc::frame::FrameHeader header{};
    const marc::core::DecoderLimits limits{};
    const marc::frame::FrameValidationContext context{
        stream, limits, 0, 0};
    ASSERT_EQ(marc::frame::parse_frame_header(
                  header_bytes, context, header),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(header.uncompressed_size, input.size());
    EXPECT_EQ(header.dictionary_serialized_size, input.size());
    EXPECT_EQ(header.entropy_block_count, 2U);
    EXPECT_EQ(header.block_descriptors_size, 288U);
    EXPECT_EQ(header.compressed_payload_size, 42U);
}

TEST(BlockedHuffmanSerializedFrameTest, RoundTripsCompleteFrame) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    const auto encoded = encoded_mixed_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size());
    const auto result = marc::frame::decode_blocked_huffman_frame(
        stream, marc::core::DecoderLimits{}, 0, 0,
        encoded, views, output);
    ASSERT_EQ(result.error, BlockedHuffmanFrameCodecError::none);
    EXPECT_EQ(result.serialized_size, encoded.size());
    EXPECT_EQ(result.output_size, input.size());
    EXPECT_EQ(output, input);
}

TEST(BlockedHuffmanSerializedFrameTest, EncodeCapacityFailureIsAtomic) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    std::array<std::byte, 385> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_blocked_huffman_frame(
        stream, marc::core::DecoderLimits{}, 0, 0, input, output);
    EXPECT_EQ(result.error, BlockedHuffmanFrameCodecError::output_too_small);
    EXPECT_EQ(result.serialized_size, 386U);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanSerializedFrameTest, StrictlyRejectsTruncationAndTrailingData) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    const auto encoded = encoded_mixed_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size());
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  views, output).error,
              BlockedHuffmanFrameCodecError::truncated_frame);

    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 0, 0,
                  extended, views, output).error,
              BlockedHuffmanFrameCodecError::trailing_frame_bytes);
}

TEST(BlockedHuffmanSerializedFrameTest, RejectsWrongSequenceAndPipeline) {
    const auto input = mixed_input();
    auto stream = stream_for(input.size());
    const auto encoded = encoded_mixed_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size());
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 1, 0,
                  encoded, views, output).error,
              BlockedHuffmanFrameCodecError::header_error);

    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.entropy_block_size = 0;
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 0, 0,
                  encoded, views, output).error,
              BlockedHuffmanFrameCodecError::unsupported_pipeline);
}

TEST(BlockedHuffmanSerializedFrameTest, ReportsViewAndOutputCapacity) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    const auto encoded = encoded_mixed_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> short_views{};
    std::vector<std::byte> output(input.size());
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 0, 0,
                  encoded, short_views, output).error,
              BlockedHuffmanFrameCodecError::view_output_too_small);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    output.resize(input.size() - 1);
    EXPECT_EQ(marc::frame::decode_blocked_huffman_frame(
                  stream, marc::core::DecoderLimits{}, 0, 0,
                  encoded, views, output).error,
              BlockedHuffmanFrameCodecError::output_too_small);
}

TEST(BlockedHuffmanSerializedFrameTest, CorruptPayloadDoesNotModifyOutput) {
    const auto input = mixed_input();
    const auto stream = stream_for(input.size());
    auto encoded = encoded_mixed_frame();
    constexpr std::size_t payload_offset =
        marc::frame::frame_header_size + 288;
    encoded[payload_offset] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::vector<std::byte> output(input.size(), std::byte{0x5a});
    const auto result = marc::frame::decode_blocked_huffman_frame(
        stream, marc::core::DecoderLimits{}, 0, 0,
        encoded, views, output);
    EXPECT_EQ(result.error,
              BlockedHuffmanFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

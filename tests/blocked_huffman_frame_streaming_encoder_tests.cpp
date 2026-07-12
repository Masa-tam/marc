#include "frame/blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/blocked_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::size_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 304;
    stream.entropy_block_size = 300;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> input_vector() {
    std::vector<std::byte> input(300, std::byte{0x41});
    input.push_back(std::byte{1});
    input.push_back(std::byte{2});
    input.push_back(std::byte{3});
    input.push_back(std::byte{4});
    input.insert(input.end(), 300, std::byte{0x42});
    return input;
}

[[nodiscard]] std::vector<std::byte> reference_encode(
    const std::vector<std::byte>& input) {
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_blocked_huffman_stream(
                  stream, marc::core::DecoderLimits{}, input, encoded).error,
              marc::frame::BlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(BlockedHuffmanFrameStreamingEncoderTest, MatchesOracleWithOneByteBuffers) {
    const auto input = input_vector();
    const auto expected = reference_encode(input);
    std::array<std::byte, 304> frame_input{};
    std::array<std::byte, 1024> frame_encoded{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        frame_input, frame_encoded};

    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto remaining = input.size() - input_offset;
        const auto count = std::min<std::size_t>(1, remaining);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    EXPECT_EQ(actual, expected);
}

TEST(BlockedHuffmanFrameStreamingEncoderTest, EmitsCompletedFrameBeforeEndInput) {
    const auto input = input_vector();
    const auto expected = reference_encode(input);
    std::array<std::byte, 304> frame_input{};
    std::array<std::byte, 1024> frame_encoded{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        frame_input, frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(304), output, 0);
    ASSERT_EQ(first.input_consumed, 304U);
    ASSERT_EQ(first.output_produced, 450U);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);

    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(304),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanFrameStreamingEncoderTest, FlushKeepsPartialFrameOpen) {
    const auto input = input_vector();
    const auto expected = reference_encode(input);
    std::array<std::byte, 304> frame_input{};
    std::array<std::byte, 1024> frame_encoded{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        frame_input, frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(100), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 100U);
    EXPECT_EQ(first.output_produced, marc::frame::stream_header_size);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);

    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(100),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanFrameStreamingEncoderTest, RequiresOnlyFrameSizedInputStorage) {
    const auto input = input_vector();
    std::array<std::byte, 304> frame_input{};
    std::array<std::byte, 1> short_encoded{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        frame_input, short_encoded};
    std::array<std::byte, 1024> output{};
    const auto result = encoder.process(
        std::span<const std::byte>{input}.first(304), output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(BlockedHuffmanFrameStreamingEncoderTest, HandlesEmptyStream) {
    std::array<std::byte, 1> unused_input{};
    std::array<std::byte, 1> unused_frame{};
    std::array<std::byte, marc::frame::stream_header_size> output{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(0), marc::core::DecoderLimits{},
        std::span<std::byte>{unused_input}.first(0), unused_frame};
    const auto result = encoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, marc::frame::stream_header_size);
}

TEST(BlockedHuffmanFrameStreamingEncoderTest, RejectsPrematureEndInput) {
    const auto input = input_vector();
    std::array<std::byte, 304> frame_input{};
    std::array<std::byte, 1024> frame_encoded{};
    marc::frame::BlockedHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        frame_input, frame_encoded};
    const auto result = encoder.process(
        std::span<const std::byte>{input}.first(10), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.input_consumed, 0U);
}

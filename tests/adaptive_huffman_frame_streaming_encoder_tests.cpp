#include "frame/adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/adaptive_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

[[nodiscard]] marc::frame::StreamHeader stream_for(const std::size_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 4;
    stream.original_size = size;
    return stream;
}

constexpr std::array input{
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}, std::byte{0x41},
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> reference_encode() {
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_adaptive_huffman_stream(
        stream, {}, input);
    EXPECT_EQ(plan.error,
              marc::frame::AdaptiveHuffmanStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_adaptive_huffman_stream(
                  stream, {}, input, output).error,
              marc::frame::AdaptiveHuffmanStreamCodecError::none);
    return output;
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, MatchesOracleWithOneByteBuffers) {
    const auto expected = reference_encode();
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 256> frame_encoded{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    std::vector<std::byte> actual;
    std::size_t offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            offset, count);
        const auto flags = offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(offset, input.size());
    EXPECT_EQ(actual, expected);
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, EmitsFullFrameBeforeEndInput) {
    const auto expected = reference_encode();
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 256> frame_encoded{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(4), output, 0);
    ASSERT_EQ(first.input_consumed, 4U);
    ASSERT_GT(first.output_produced, marc::frame::stream_header_size);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(4),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, FlushKeepsPartialFrameOpen) {
    const auto expected = reference_encode();
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 256> frame_encoded{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(2), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 2U);
    EXPECT_EQ(first.output_produced, marc::frame::stream_header_size);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(2),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, ReportsShortFrameWorkspace) {
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 1> short_frame{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, short_frame};
    std::array<std::byte, 512> output{};
    const auto result = encoder.process(
        std::span<const std::byte>{input}.first(4), output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, PreservesPlanningLimitCategory) {
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 256> frame_encoded{};
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 2;
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), limits, frame_input, frame_encoded};
    std::array<std::byte, 512> output{};
    const auto result = encoder.process(
        std::span<const std::byte>{input}.first(4), output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(AdaptiveHuffmanFrameStreamingEncoder, HandlesEmptyAndRejectsPrematureEnd) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, marc::frame::stream_header_size> output{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder empty{
        stream_for(0), {}, std::span<std::byte>{unused}.first(0), unused};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, marc::frame::stream_header_size);

    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 256> frame_encoded{};
    marc::frame::AdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    result = encoder.process(
        std::span<const std::byte>{input}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.input_consumed, 0U);
}

} // namespace

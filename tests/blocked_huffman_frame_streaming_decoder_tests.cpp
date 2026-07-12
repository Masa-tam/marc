#include "frame/blocked_huffman_frame_streaming_decoder.hpp"
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

[[nodiscard]] std::vector<std::byte> encode(
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

TEST(BlockedHuffmanFrameStreamingDecoderTest, HandlesOneByteInputAndOutput) {
    const auto expected = input_vector();
    const auto encoded = encode(expected);
    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};

    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto remaining = encoded.size() - input_offset;
        const auto count = std::min<std::size_t>(1, remaining);
        const auto chunk = std::span<const std::byte>{encoded}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == encoded.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_EQ(actual, expected);
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, EmitsValidatedFrameBeforeEndInput) {
    const auto expected = input_vector();
    const auto encoded = encode(expected);
    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};
    std::array<std::byte, 304> first_output{};
    const auto first = decoder.process(
        std::span<const std::byte>{encoded}.first(450), first_output, 0);
    ASSERT_EQ(first.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(first.input_consumed, 450U);
    EXPECT_EQ(first.output_produced, 304U);
    EXPECT_TRUE(std::ranges::equal(
        first_output, std::span<const std::byte>{expected}.first(304)));
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, LaterCorruptionKeepsPriorFrameOnly) {
    const auto expected = input_vector();
    auto encoded = encode(expected);
    constexpr std::size_t second_payload = 64 + 386 + 56 + 272;
    encoded[second_payload] = std::byte{1};
    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};
    std::vector<std::byte> output(expected.size(), std::byte{0x5a});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 304U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(304),
        std::span<const std::byte>{expected}.first(304)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(304),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, EndInputRejectsTruncation) {
    const auto expected = input_vector();
    const auto encoded = encode(expected);
    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};
    const auto truncated =
        std::span<const std::byte>{encoded}.first(encoded.size() - 1);
    const auto first = decoder.process(
        truncated, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(first.status, marc::core::StreamStatus::need_output);
    std::array<std::byte, 304> committed_output{};
    const auto second = decoder.process(
        truncated.subspan(first.input_consumed), committed_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::error);
    EXPECT_EQ(second.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, ReportsFrameWorkspaceLimits) {
    const auto expected = input_vector();
    const auto encoded = encode(expected);
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder encoded_short{
        marc::core::DecoderLimits{}, short_frame, frame_decoded, views};
    const auto first = encoded_short.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(first.status, marc::core::StreamStatus::error);
    EXPECT_EQ(first.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 303> short_decoded{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoded_short{
        marc::core::DecoderLimits{}, frame_encoded, short_decoded, views};
    const auto second = decoded_short.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, HandlesEmptyStream) {
    const std::vector<std::byte> expected;
    const auto encoded = encode(expected);
    std::array<std::byte, 1> frame_encoded{};
    std::array<std::byte, 1> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 0> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};
    const auto result = decoder.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 0U);
}

TEST(BlockedHuffmanFrameStreamingDecoderTest, RejectsTrailingFinalBytes) {
    const auto expected = input_vector();
    auto encoded = encode(expected);
    encoded.push_back(std::byte{0});
    std::array<std::byte, 1024> frame_encoded{};
    std::array<std::byte, 304> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    marc::frame::BlockedHuffmanFrameStreamingDecoder decoder{
        marc::core::DecoderLimits{}, frame_encoded, frame_decoded, views};
    const auto first = decoder.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(first.status, marc::core::StreamStatus::need_output);
    std::array<std::byte, 304> committed_output{};
    const auto second = decoder.process(
        std::span<const std::byte>{encoded}.subspan(first.input_consumed),
        committed_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::error);
    EXPECT_EQ(second.error.code, marc::core::ErrorCode::malformed_stream);
}

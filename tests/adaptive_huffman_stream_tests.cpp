#include "frame/adaptive_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::AdaptiveHuffmanStreamCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size, const std::uint32_t frame_size = 2) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = frame_size;
    stream.original_size = size;
    return stream;
}

constexpr std::array repeated_a{
    std::byte{0x41}, std::byte{0x41},
    std::byte{0x41}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> encoded_repeated_a() {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_adaptive_huffman_stream(
        stream, {}, repeated_a);
    EXPECT_EQ(plan.error, AdaptiveHuffmanStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_adaptive_huffman_stream(
                  stream, {}, repeated_a, output).error,
              AdaptiveHuffmanStreamCodecError::none);
    return output;
}

TEST(AdaptiveHuffmanStream, RoundTripsMultipleFrames) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_adaptive_huffman_stream(
        stream, {}, repeated_a);
    ASSERT_EQ(plan.error, AdaptiveHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 212U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    marc::frame::StreamHeader decoded_stream{};
    const auto result = marc::frame::decode_adaptive_huffman_stream(
        encoded, {}, output, decoded_stream);
    ASSERT_EQ(result.error, AdaptiveHuffmanStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_stream.original_size, repeated_a.size());
}

TEST(AdaptiveHuffmanStream, ResetsModelAtEveryFrame) {
    const auto encoded = encoded_repeated_a();
    constexpr std::size_t first_payload = 64 + 56 + 16;
    constexpr std::size_t second_payload = 64 + 74 + 56 + 16;
    constexpr std::array expected{std::byte{0x41}, std::byte{0x01}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(first_payload, 2),
        expected));
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(second_payload, 2),
        expected));
}

TEST(AdaptiveHuffmanStream, EmptyInputIsHeaderOnly) {
    const std::array<std::byte, 0> input{};
    const auto stream = stream_for(0);
    const auto plan = marc::frame::plan_adaptive_huffman_stream(
        stream, {}, input);
    ASSERT_EQ(plan.error, AdaptiveHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, marc::frame::stream_header_size);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, marc::frame::stream_header_size> encoded{};
    ASSERT_EQ(marc::frame::encode_adaptive_huffman_stream(
                  stream, {}, input, encoded).error,
              AdaptiveHuffmanStreamCodecError::none);
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_stream(
                  encoded, {}, {}, decoded).error,
              AdaptiveHuffmanStreamCodecError::none);
}

TEST(AdaptiveHuffmanStream, PlansBeforeWritingAnyOutput) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_adaptive_huffman_stream(
        stream, {}, repeated_a);
    std::vector<std::byte> output(plan.serialized_size - 1, std::byte{0x5a});
    const auto result = marc::frame::encode_adaptive_huffman_stream(
        stream, {}, repeated_a, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(AdaptiveHuffmanStream, LaterFrameCorruptionLeavesOutputUntouched) {
    auto encoded = encoded_repeated_a();
    constexpr std::size_t second_payload_last = 64 + 74 + 56 + 16 + 1;
    encoded[second_payload_last] |= std::byte{0x80};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5a});
    marc::frame::StreamHeader decoded{};
    const auto result = marc::frame::decode_adaptive_huffman_stream(
        encoded, {}, output, decoded);
    EXPECT_EQ(result.error, AdaptiveHuffmanStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(AdaptiveHuffmanStream, RejectsTruncationAndTrailingBytes) {
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, output, decoded).error,
              AdaptiveHuffmanStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_stream(
                  extended, {}, output, decoded).error,
              AdaptiveHuffmanStreamCodecError::trailing_stream_bytes);
}

TEST(AdaptiveHuffmanStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = stream_for(repeated_a.size() + 1);
    EXPECT_EQ(marc::frame::plan_adaptive_huffman_stream(
                  stream, {}, repeated_a).error,
              AdaptiveHuffmanStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_stream(
                  encoded, {}, output, decoded).error,
              AdaptiveHuffmanStreamCodecError::output_too_small);
}

} // namespace

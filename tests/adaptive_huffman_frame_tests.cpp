#include "frame/adaptive_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::AdaptiveHuffmanFrameCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = static_cast<std::uint32_t>(size);
    stream.original_size = size;
    return stream;
}

constexpr std::array aba{
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> encoded_aba() {
    const auto stream = stream_for(aba.size());
    const auto plan = marc::frame::plan_adaptive_huffman_frame(
        stream, {}, 0, 0, aba);
    EXPECT_EQ(plan.error, AdaptiveHuffmanFrameCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_adaptive_huffman_frame(
                  stream, {}, 0, 0, aba, output).error,
              AdaptiveHuffmanFrameCodecError::none);
    return output;
}

TEST(AdaptiveHuffmanFrame, PlansHeaderDescriptorAndPayload) {
    const auto result = marc::frame::plan_adaptive_huffman_frame(
        stream_for(aba.size()), {}, 0, 0, aba);
    EXPECT_EQ(result.error, AdaptiveHuffmanFrameCodecError::none);
    EXPECT_EQ(result.serialized_size, 75U);
    EXPECT_EQ(result.output_size, aba.size());
}

TEST(AdaptiveHuffmanFrame, EncodesCanonicalRegions) {
    const auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    const std::span<const std::byte, marc::frame::frame_header_size> header_bytes{
        encoded.data(), marc::frame::frame_header_size};
    marc::frame::FrameHeader header{};
    const marc::core::DecoderLimits limits{};
    ASSERT_EQ(marc::frame::parse_frame_header(
                  header_bytes, {stream, limits, 0, 0}, header),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(header.uncompressed_size, 3U);
    EXPECT_EQ(header.dictionary_serialized_size, 3U);
    EXPECT_EQ(header.compressed_payload_size, 3U);
    EXPECT_EQ(header.entropy_block_count, 1U);
    EXPECT_EQ(header.block_descriptors_size, 16U);
    constexpr std::array descriptor{
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(56, 16), descriptor));
    constexpr std::array payload{
        std::byte{0x41}, std::byte{0x84}, std::byte{0x02}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(72), payload));
}

TEST(AdaptiveHuffmanFrame, RoundTripsCompleteFrame) {
    const auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    const auto result = marc::frame::decode_adaptive_huffman_frame(
        stream, {}, 0, 0, encoded, output);
    ASSERT_EQ(result.error, AdaptiveHuffmanFrameCodecError::none);
    EXPECT_EQ(result.serialized_size, encoded.size());
    EXPECT_EQ(result.output_size, aba.size());
    EXPECT_EQ(output, aba);
}

TEST(AdaptiveHuffmanFrame, EncodeCapacityFailureIsAtomic) {
    const auto stream = stream_for(aba.size());
    std::array<std::byte, 74> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_adaptive_huffman_frame(
        stream, {}, 0, 0, aba, output);
    EXPECT_EQ(result.error, AdaptiveHuffmanFrameCodecError::output_too_small);
    EXPECT_EQ(result.serialized_size, 75U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(AdaptiveHuffmanFrame, StrictlyRejectsTruncationAndTrailingBytes) {
    const auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output).error,
              AdaptiveHuffmanFrameCodecError::truncated_frame);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 0, 0, extended, output).error,
              AdaptiveHuffmanFrameCodecError::trailing_frame_bytes);
}

TEST(AdaptiveHuffmanFrame, RejectsMalformedDescriptorAndPayloadAtomically) {
    const auto stream = stream_for(aba.size());
    auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    output.fill(std::byte{0x5a});
    encoded[56 + 15] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 0, 0, encoded, output).error,
              AdaptiveHuffmanFrameCodecError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    encoded = encoded_aba();
    encoded.back() |= std::byte{0x80};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 0, 0, encoded, output).error,
              AdaptiveHuffmanFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(AdaptiveHuffmanFrame, RejectsWrongBoundaryAndPipeline) {
    auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 1, 0, encoded, output).error,
              AdaptiveHuffmanFrameCodecError::header_error);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::decode_adaptive_huffman_frame(
                  stream, {}, 0, 0, encoded, output).error,
              AdaptiveHuffmanFrameCodecError::unsupported_pipeline);
}

} // namespace

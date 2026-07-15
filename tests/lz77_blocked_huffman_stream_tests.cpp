#include "frame/lz77_blocked_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace {

using marc::frame::Lz77BlockedHuffmanStreamCodecError;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 16;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_ababx() {
    const auto raw = bytes("ABABX");
    const auto stream = stream_for(raw.size());
    std::array<std::byte, 32> staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_stream(
        stream, {}, {}, raw, staging);
    EXPECT_EQ(plan.error, Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 408U);
    EXPECT_EQ(plan.frame_count, 3U);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw, staging, encoded)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(Lz77BlockedHuffmanStream, PlansEncodesAndDecodesMultipleFrames) {
    const auto raw = bytes("ABABX");
    const auto stream = stream_for(raw.size());
    std::array<std::byte, 32> encode_staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_stream(
        stream, {}, {}, raw, encode_staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 408U);
    EXPECT_EQ(plan.output_size, raw.size());
    EXPECT_EQ(plan.frame_count, 3U);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw, encode_staging, encoded)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<std::byte, 32> decode_staging{};
    std::vector<std::byte> decoded(raw.size());
    marc::frame::StreamHeader parsed{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    const auto result = marc::frame::decode_lz77_blocked_huffman_stream(
        encoded, {}, views, decode_staging, decoded, parsed, parameters);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 3U);
    EXPECT_EQ(decoded, raw);
    EXPECT_EQ(parsed.original_size, raw.size());
    EXPECT_EQ(parameters.window_size,
              marc::dictionary::internal::lz77_default_window_size);
}

TEST(Lz77BlockedHuffmanStream, ResetsBothLayersAtFrameBoundaries) {
    const auto encoded = encoded_ababx();
    constexpr std::size_t prefix = marc::frame::stream_header_size
        + marc::dictionary::internal::lz77_parameter_size;
    constexpr std::size_t frame_size = 120;
    constexpr std::size_t body_offset = marc::frame::frame_header_size;
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(
            prefix + body_offset, frame_size - body_offset),
        std::span<const std::byte>{encoded}.subspan(
            prefix + frame_size + body_offset,
            frame_size - body_offset)));
}

TEST(Lz77BlockedHuffmanStream, EmptyInputIsCanonicalPrefixOnly) {
    const auto stream = stream_for(0);
    std::array<std::byte, 1> staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_stream(
        stream, {}, {}, {}, staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, {}, staging, encoded)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);

    marc::frame::StreamHeader parsed{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    ASSERT_EQ(marc::frame::decode_lz77_blocked_huffman_stream(
                  encoded, {}, {}, {}, {}, parsed, parameters)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(parsed.original_size, 0U);
}

TEST(Lz77BlockedHuffmanStream, EncodingIsDeterministicAndCapacityAtomic) {
    const auto raw = bytes("ABABX");
    const auto stream = stream_for(raw.size());
    std::array<std::byte, 32> staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_stream(
        stream, {}, {}, raw, staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanStreamCodecError::none);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw, staging, first)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw, staging, second)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    EXPECT_EQ(first, second);

    second.resize(second.size() - 1);
    std::ranges::fill(second, std::byte{0x5a});
    EXPECT_EQ(marc::frame::encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw, staging, second)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(second, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77BlockedHuffmanStream, LaterFrameCorruptionIsWholeStreamAtomic) {
    auto encoded = encoded_ababx();
    constexpr std::size_t second_frame_payload = 80 + 120 + 56 + 32;
    encoded[second_frame_payload] = std::byte{0xff};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<std::byte, 32> staging{};
    std::array<std::byte, 5> output{};
    output.fill(std::byte{0x5a});
    marc::frame::StreamHeader parsed{};
    parsed.original_size = 99;
    marc::dictionary::internal::Lz77Parameters parameters{};
    parameters.window_size = 7;
    const auto result = marc::frame::decode_lz77_blocked_huffman_stream(
        encoded, {}, views, staging, output, parsed, parameters);
    EXPECT_EQ(result.error, Lz77BlockedHuffmanStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
    EXPECT_EQ(parsed.original_size, 99U);
    EXPECT_EQ(parameters.window_size, 7U);
}

TEST(Lz77BlockedHuffmanStream, StrictlyRejectsTruncationAndTrailingBytes) {
    const auto encoded = encoded_ababx();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<std::byte, 32> staging{};
    std::array<std::byte, 5> output{};
    marc::frame::StreamHeader parsed{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        EXPECT_NE(marc::frame::decode_lz77_blocked_huffman_stream(
                      std::span<const std::byte>{encoded}.first(size), {},
                      views, staging, output, parsed, parameters)
                      .error,
                  Lz77BlockedHuffmanStreamCodecError::none)
            << size;
    }
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_stream(
                  extended, {}, views, staging, output, parsed, parameters)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::trailing_stream_bytes);
}

TEST(Lz77BlockedHuffmanStream, ReportsIndependentWorkspaceAndOutputLimits) {
    const auto encoded = encoded_ababx();
    const auto raw = bytes("ABABX");
    const auto stream = stream_for(raw.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<std::byte, 32> staging{};
    std::array<std::byte, 5> output{};
    marc::frame::StreamHeader parsed{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_stream(
                  encoded, {}, views, staging,
                  std::span<std::byte>{output}.first<4>(), parsed,
                  parameters)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::output_too_small);
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_stream(
                  encoded, {},
                  std::span<marc::entropy::internal::
                                BlockedHuffmanBlockView>{views}.first<1>(),
                  staging, output, parsed,
                  parameters)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::view_output_too_small);
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_stream(
                  encoded, {}, views,
                  std::span<std::byte>{staging}.first<31>(), output, parsed,
                  parameters)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::
                  dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::plan_lz77_blocked_huffman_stream(
                  stream, {}, {}, raw,
                  std::span<std::byte>{staging}.first<31>())
                  .error,
              Lz77BlockedHuffmanStreamCodecError::
                  dictionary_staging_too_small);
}

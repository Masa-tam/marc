#include "frame/tans_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::TansFrameCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for() {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = 3;
    stream.entropy_block_size = 2;
    stream.original_size = 3;
    return stream;
}

constexpr std::array aba{
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> encoded_aba() {
    const auto stream = stream_for();
    const auto plan = marc::frame::plan_tans_frame(
        stream, {}, 0, 0, aba);
    EXPECT_EQ(plan.error, TansFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_tans_frame(
                  stream, {}, 0, 0, aba, encoded).error,
              TansFrameCodecError::none);
    return encoded;
}

TEST(TansFrame, PlansAndEncodesTwoCanonicalBlocks) {
    const auto stream = stream_for();
    const auto plan = marc::frame::plan_tans_frame(
        stream, {}, 0, 0, aba);
    ASSERT_EQ(plan.error, TansFrameCodecError::none);
    EXPECT_EQ(plan.block_count, 2U);
    EXPECT_EQ(plan.serialized_size, 1117U);
    const auto encoded = encoded_aba();
    const std::span<const std::byte, marc::frame::frame_header_size> header_bytes{
        encoded.data(), marc::frame::frame_header_size};
    marc::frame::FrameHeader header{};
    const marc::core::DecoderLimits limits{};
    ASSERT_EQ(marc::frame::parse_frame_header(
                  header_bytes, {stream, limits, 0, 0}, header),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(header.entropy_block_count, 2U);
    EXPECT_EQ(header.block_descriptors_size, 1056U);
    EXPECT_EQ(header.compressed_payload_size, 5U);
    constexpr std::array first_payload{
        std::byte{0x06}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(56 + 1056, 3),
        first_payload));
}

TEST(TansFrame, RoundTripsAndValidatesCompleteFrame) {
    const auto stream = stream_for();
    const auto encoded = encoded_aba();
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 0, 0, encoded, views).error,
              TansFrameCodecError::none);
    std::array<std::byte, aba.size()> output{};
    const auto decoded = marc::frame::decode_tans_frame(
        stream, {}, 0, 0, encoded, output, views);
    EXPECT_EQ(decoded.error, TansFrameCodecError::none);
    EXPECT_EQ(output, aba);
}

TEST(TansFrame, CapacityAndMalformedBlocksAreAtomic) {
    const auto stream = stream_for();
    std::vector<std::byte> short_output(1116, std::byte{0x5a});
    EXPECT_EQ(marc::frame::encode_tans_frame(
                  stream, {}, 0, 0, aba, short_output).error,
              TansFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto encoded = encoded_aba();
    encoded[56 + 528 + 15] = std::byte{1};
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    std::array<std::byte, aba.size()> output{};
    output.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_tans_frame(
                  stream, {}, 0, 0, encoded, output, views).error,
              TansFrameCodecError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    encoded = encoded_aba();
    encoded[56 + 1056 + 3] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_tans_frame(
                  stream, {}, 0, 0, encoded, output, views).error,
              TansFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(TansFrame, RejectsViewsExtentBoundaryAndPipelineErrors) {
    auto stream = stream_for();
    const auto encoded = encoded_aba();
    std::array<marc::entropy::internal::TansBlockView, 1> short_views{};
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 0, 0, encoded, short_views).error,
              TansFrameCodecError::views_too_small);
    std::array<marc::entropy::internal::TansBlockView, 2> views{};
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  views).error,
              TansFrameCodecError::truncated_frame);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 0, 0, extended, views).error,
              TansFrameCodecError::trailing_frame_bytes);
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 1, 0, encoded, views).error,
              TansFrameCodecError::header_error);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_tans_frame(
                  stream, {}, 0, 0, encoded, views).error,
              TansFrameCodecError::unsupported_pipeline);
}

} // namespace

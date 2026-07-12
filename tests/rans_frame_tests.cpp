#include "frame/rans_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::RansFrameCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for() {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
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
    const auto plan = marc::frame::plan_rans_frame(
        stream, {}, 0, 0, aba);
    EXPECT_EQ(plan.error, RansFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_rans_frame(
                  stream, {}, 0, 0, aba, encoded).error,
              RansFrameCodecError::none);
    return encoded;
}

TEST(RansFrame, PlansAndEncodesTwoCanonicalBlocks) {
    const auto stream = stream_for();
    const auto plan = marc::frame::plan_rans_frame(
        stream, {}, 0, 0, aba);
    ASSERT_EQ(plan.error, RansFrameCodecError::none);
    EXPECT_EQ(plan.block_count, 2U);
    EXPECT_EQ(plan.serialized_size, 1128U);
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
    EXPECT_EQ(header.compressed_payload_size, 16U);
    constexpr std::array first_payload{
        std::byte{0x00}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(56 + 1056, 8),
        first_payload));
}

TEST(RansFrame, RoundTripsAndValidatesCompleteFrame) {
    const auto stream = stream_for();
    const auto encoded = encoded_aba();
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 0, 0, encoded, views).error,
              RansFrameCodecError::none);
    std::array<std::byte, aba.size()> output{};
    const auto decoded = marc::frame::decode_rans_frame(
        stream, {}, 0, 0, encoded, output, views);
    EXPECT_EQ(decoded.error, RansFrameCodecError::none);
    EXPECT_EQ(output, aba);
}

TEST(RansFrame, CapacityAndMalformedBlocksAreAtomic) {
    const auto stream = stream_for();
    std::vector<std::byte> short_output(1127, std::byte{0x5a});
    EXPECT_EQ(marc::frame::encode_rans_frame(
                  stream, {}, 0, 0, aba, short_output).error,
              RansFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto encoded = encoded_aba();
    encoded[56 + 528 + 15] = std::byte{1};
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    std::array<std::byte, aba.size()> output{};
    output.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_rans_frame(
                  stream, {}, 0, 0, encoded, output, views).error,
              RansFrameCodecError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    encoded = encoded_aba();
    encoded[56 + 1056 + 8 + 3] = std::byte{0};
    EXPECT_EQ(marc::frame::decode_rans_frame(
                  stream, {}, 0, 0, encoded, output, views).error,
              RansFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(RansFrame, RejectsViewsExtentBoundaryAndPipelineErrors) {
    auto stream = stream_for();
    const auto encoded = encoded_aba();
    std::array<marc::entropy::internal::RansBlockView, 1> short_views{};
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 0, 0, encoded, short_views).error,
              RansFrameCodecError::views_too_small);
    std::array<marc::entropy::internal::RansBlockView, 2> views{};
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  views).error,
              RansFrameCodecError::truncated_frame);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 0, 0, extended, views).error,
              RansFrameCodecError::trailing_frame_bytes);
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 1, 0, encoded, views).error,
              RansFrameCodecError::header_error);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_rans_frame(
                  stream, {}, 0, 0, encoded, views).error,
              RansFrameCodecError::unsupported_pipeline);
}

} // namespace

#include "frame/dynamic_range_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::DynamicRangeFrameCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(const std::uint64_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = static_cast<std::uint32_t>(size);
    stream.original_size = size;
    return stream;
}

constexpr std::array aba{
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> encoded_aba() {
    const auto stream = stream_for(aba.size());
    const auto plan = marc::frame::plan_dynamic_range_frame(
        stream, {}, 0, 0, aba);
    EXPECT_EQ(plan.error, DynamicRangeFrameCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_dynamic_range_frame(
                  stream, {}, 0, 0, aba, output).error,
              DynamicRangeFrameCodecError::none);
    return output;
}

TEST(DynamicRangeFrame, PlansAndEncodesCanonicalRegions) {
    const auto stream = stream_for(aba.size());
    const auto plan = marc::frame::plan_dynamic_range_frame(
        stream, {}, 0, 0, aba);
    ASSERT_EQ(plan.error, DynamicRangeFrameCodecError::none);
    EXPECT_EQ(plan.serialized_size, 79U);
    EXPECT_EQ(plan.output_size, aba.size());

    const auto encoded = encoded_aba();
    const std::span<const std::byte, marc::frame::frame_header_size> header_bytes{
        encoded.data(), marc::frame::frame_header_size};
    marc::frame::FrameHeader header{};
    const marc::core::DecoderLimits limits{};
    ASSERT_EQ(marc::frame::parse_frame_header(
                  header_bytes, {stream, limits, 0, 0}, header),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(header.uncompressed_size, 3U);
    EXPECT_EQ(header.compressed_payload_size, 7U);
    EXPECT_EQ(header.entropy_block_count, 1U);
    EXPECT_EQ(header.block_descriptors_size, 16U);
    constexpr std::array descriptor{
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{7}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(56, 16), descriptor));
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x42}, std::byte{0xfd},
        std::byte{0x40}, std::byte{0x3c}, std::byte{0xf0}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(72), payload));
}

TEST(DynamicRangeFrame, RoundTripsCompleteFrame) {
    const auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    const auto result = marc::frame::decode_dynamic_range_frame(
        stream, {}, 0, 0, encoded, output);
    ASSERT_EQ(result.error, DynamicRangeFrameCodecError::none);
    EXPECT_EQ(result.serialized_size, encoded.size());
    EXPECT_EQ(result.output_size, aba.size());
    EXPECT_EQ(output, aba);
    EXPECT_EQ(marc::frame::validate_dynamic_range_frame(
                  stream, {}, 0, 0, encoded).error,
              DynamicRangeFrameCodecError::none);
}

TEST(DynamicRangeFrame, CapacityAndMalformedInputAreAtomic) {
    const auto stream = stream_for(aba.size());
    std::array<std::byte, 78> short_output{};
    short_output.fill(std::byte{0x5a});
    auto result = marc::frame::encode_dynamic_range_frame(
        stream, {}, 0, 0, aba, short_output);
    EXPECT_EQ(result.error, DynamicRangeFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    output.fill(std::byte{0x5a});
    encoded[56 + 15] = std::byte{1};
    result = marc::frame::decode_dynamic_range_frame(
        stream, {}, 0, 0, encoded, output);
    EXPECT_EQ(result.error, DynamicRangeFrameCodecError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    encoded = encoded_aba();
    encoded[72] = std::byte{0xff};
    result = marc::frame::decode_dynamic_range_frame(
        stream, {}, 0, 0, encoded, output);
    EXPECT_EQ(result.error, DynamicRangeFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(DynamicRangeFrame, RejectsExtentBoundaryAndPipelineErrors) {
    auto stream = stream_for(aba.size());
    const auto encoded = encoded_aba();
    std::array<std::byte, aba.size()> output{};
    EXPECT_EQ(marc::frame::decode_dynamic_range_frame(
                  stream, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output).error,
              DynamicRangeFrameCodecError::truncated_frame);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_dynamic_range_frame(
                  stream, {}, 0, 0, extended, output).error,
              DynamicRangeFrameCodecError::trailing_frame_bytes);
    EXPECT_EQ(marc::frame::decode_dynamic_range_frame(
                  stream, {}, 1, 0, encoded, output).error,
              DynamicRangeFrameCodecError::header_error);
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::decode_dynamic_range_frame(
                  stream, {}, 0, 0, encoded, output).error,
              DynamicRangeFrameCodecError::unsupported_pipeline);
}

} // namespace

#include "frame/dynamic_range_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::DynamicRangeStreamCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size, const std::uint32_t frame_size = 2) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::dynamic_range;
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
    const auto plan = marc::frame::plan_dynamic_range_stream(
        stream, {}, repeated_a);
    EXPECT_EQ(plan.error, DynamicRangeStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_dynamic_range_stream(
                  stream, {}, repeated_a, output).error,
              DynamicRangeStreamCodecError::none);
    return output;
}

TEST(DynamicRangeStream, RoundTripsMultipleFrames) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_dynamic_range_stream(
        stream, {}, repeated_a);
    ASSERT_EQ(plan.error, DynamicRangeStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 220U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    marc::frame::StreamHeader decoded_stream{};
    const auto result = marc::frame::decode_dynamic_range_stream(
        encoded, {}, output, decoded_stream);
    ASSERT_EQ(result.error, DynamicRangeStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_stream.original_size, repeated_a.size());
}

TEST(DynamicRangeStream, ResetsModelAtEveryFrame) {
    const auto encoded = encoded_repeated_a();
    constexpr std::size_t first_payload = 64 + 56 + 16;
    constexpr std::size_t second_payload = 64 + 78 + 56 + 16;
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x41}, std::byte{0x40},
        std::byte{0xbe}, std::byte{0xff}, std::byte{0x7e}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(first_payload, 6),
        expected));
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(second_payload, 6),
        expected));
}

TEST(DynamicRangeStream, EmptyInputIsHeaderOnly) {
    const std::array<std::byte, 0> input{};
    const auto stream = stream_for(0);
    const auto plan = marc::frame::plan_dynamic_range_stream(
        stream, {}, input);
    ASSERT_EQ(plan.error, DynamicRangeStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, marc::frame::stream_header_size);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, marc::frame::stream_header_size> encoded{};
    ASSERT_EQ(marc::frame::encode_dynamic_range_stream(
                  stream, {}, input, encoded).error,
              DynamicRangeStreamCodecError::none);
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_dynamic_range_stream(
                  encoded, {}, {}, decoded).error,
              DynamicRangeStreamCodecError::none);
}

TEST(DynamicRangeStream, PlansBeforeWritingAnyOutput) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_dynamic_range_stream(
        stream, {}, repeated_a);
    std::vector<std::byte> output(plan.serialized_size - 1, std::byte{0x5a});
    const auto result = marc::frame::encode_dynamic_range_stream(
        stream, {}, repeated_a, output);
    EXPECT_EQ(result.error, DynamicRangeStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(DynamicRangeStream, LaterFrameCorruptionLeavesOutputUntouched) {
    auto encoded = encoded_repeated_a();
    constexpr std::size_t second_payload_first = 64 + 78 + 56 + 16;
    encoded[second_payload_first] = std::byte{0xff};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5a});
    marc::frame::StreamHeader decoded{};
    const auto result = marc::frame::decode_dynamic_range_stream(
        encoded, {}, output, decoded);
    EXPECT_EQ(result.error, DynamicRangeStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(DynamicRangeStream, RejectsTruncationAndTrailingBytes) {
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_dynamic_range_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, output, decoded).error,
              DynamicRangeStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_dynamic_range_stream(
                  extended, {}, output, decoded).error,
              DynamicRangeStreamCodecError::trailing_stream_bytes);
}

TEST(DynamicRangeStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = stream_for(repeated_a.size() + 1);
    EXPECT_EQ(marc::frame::plan_dynamic_range_stream(
                  stream, {}, repeated_a).error,
              DynamicRangeStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    marc::frame::StreamHeader decoded{};
    EXPECT_EQ(marc::frame::decode_dynamic_range_stream(
                  encoded, {}, output, decoded).error,
              DynamicRangeStreamCodecError::output_too_small);
}

} // namespace

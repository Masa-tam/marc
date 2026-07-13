#include "frame/tans_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::TansStreamCodecError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint64_t size, const std::uint32_t frame_size = 2) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = frame_size;
    stream.entropy_block_size = 2;
    stream.original_size = size;
    return stream;
}

constexpr std::array repeated_a{
    std::byte{0x41}, std::byte{0x41},
    std::byte{0x41}, std::byte{0x41}};

[[nodiscard]] std::vector<std::byte> encoded_repeated_a() {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_tans_stream(
        stream, {}, repeated_a);
    EXPECT_EQ(plan.error, TansStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_tans_stream(
                  stream, {}, repeated_a, output).error,
              TansStreamCodecError::none);
    return output;
}

TEST(TansStream, RoundTripsMultipleFrames) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_tans_stream(
        stream, {}, repeated_a);
    ASSERT_EQ(plan.error, TansStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 1236U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    marc::frame::StreamHeader decoded_stream{};
    const auto result = marc::frame::decode_tans_stream(
        encoded, {}, output, views, decoded_stream);
    ASSERT_EQ(result.error, TansStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_stream.original_size, repeated_a.size());
}

TEST(TansStream, ResetsModelAtEveryFrame) {
    const auto encoded = encoded_repeated_a();
    constexpr std::size_t first_payload = 64 + 56 + 528;
    constexpr std::size_t second_payload = 64 + 586 + 56 + 528;
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(first_payload, 2),
        expected));
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(second_payload, 2),
        expected));
}

TEST(TansStream, EmptyInputIsHeaderOnly) {
    const std::array<std::byte, 0> input{};
    const auto stream = stream_for(0);
    const auto plan = marc::frame::plan_tans_stream(
        stream, {}, input);
    ASSERT_EQ(plan.error, TansStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, marc::frame::stream_header_size);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, marc::frame::stream_header_size> encoded{};
    ASSERT_EQ(marc::frame::encode_tans_stream(
                  stream, {}, input, encoded).error,
              TansStreamCodecError::none);
    marc::frame::StreamHeader decoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::decode_tans_stream(
                  encoded, {}, {}, views, decoded).error,
              TansStreamCodecError::none);
}

TEST(TansStream, PlansBeforeWritingAnyOutput) {
    const auto stream = stream_for(repeated_a.size());
    const auto plan = marc::frame::plan_tans_stream(
        stream, {}, repeated_a);
    std::vector<std::byte> output(plan.serialized_size - 1, std::byte{0x5a});
    const auto result = marc::frame::encode_tans_stream(
        stream, {}, repeated_a, output);
    EXPECT_EQ(result.error, TansStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(TansStream, LaterFrameCorruptionLeavesOutputUntouched) {
    auto encoded = encoded_repeated_a();
    constexpr std::size_t second_payload_first = 64 + 586 + 56 + 528;
    encoded[second_payload_first] = std::byte{1};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5a});
    marc::frame::StreamHeader decoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    const auto result = marc::frame::decode_tans_stream(
        encoded, {}, output, views, decoded);
    EXPECT_EQ(result.error, TansStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(TansStream, RejectsTruncationAndTrailingBytes) {
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    marc::frame::StreamHeader decoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::decode_tans_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, output, views, decoded).error,
              TansStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::decode_tans_stream(
                  extended, {}, output, views, decoded).error,
              TansStreamCodecError::trailing_stream_bytes);
}

TEST(TansStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = stream_for(repeated_a.size() + 1);
    EXPECT_EQ(marc::frame::plan_tans_stream(
                  stream, {}, repeated_a).error,
              TansStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    marc::frame::StreamHeader decoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    EXPECT_EQ(marc::frame::decode_tans_stream(
                  encoded, {}, output, views, decoded).error,
              TansStreamCodecError::output_too_small);
}

} // namespace

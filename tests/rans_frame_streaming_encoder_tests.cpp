#include "frame/rans_frame_streaming_encoder.hpp"
#include "frame/rans_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace {
constexpr std::array input{
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}, std::byte{0x41},
    std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};

[[nodiscard]] marc::frame::StreamHeader stream_for(const std::size_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = 4;
    stream.entropy_block_size = 2;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference_encode() {
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_rans_stream(stream, {}, input);
    EXPECT_EQ(plan.error, marc::frame::RansStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_rans_stream(stream, {}, input, output).error,
              marc::frame::RansStreamCodecError::none);
    return output;
}

TEST(RansFrameStreamingEncoder, MatchesOracleWithOneByteBuffers) {
    const auto expected = reference_encode();
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 1200> frame_encoded{};
    marc::frame::RansFrameStreamingEncoder encoder{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    std::vector<std::byte> actual;
    std::size_t offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(offset, count);
        const auto flags = offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(offset, input.size());
    EXPECT_EQ(actual, expected);
}

TEST(RansFrameStreamingEncoder, EmitsFullFrameAndKeepsFlushOpen) {
    const auto expected = reference_encode();
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 1200> frame_encoded{};
    marc::frame::RansFrameStreamingEncoder encoder{
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
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(RansFrameStreamingEncoder, ReportsWorkspaceLimitsAndPrematureEnd) {
    std::array<std::byte, 4> frame_input{};
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 2048> output{};
    marc::frame::RansFrameStreamingEncoder short_encoder{
        stream_for(input.size()), {}, frame_input, short_frame};
    auto result = short_encoder.process(
        std::span<const std::byte>{input}.first(4), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 1;
    std::array<std::byte, 1200> frame_encoded{};
    marc::frame::RansFrameStreamingEncoder limited{
        stream_for(input.size()), limits, frame_input, frame_encoded};
    result = limited.process(std::span<const std::byte>{input}.first(4), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);

    marc::frame::RansFrameStreamingEncoder premature{
        stream_for(input.size()), {}, frame_input, frame_encoded};
    result = premature.process(
        std::span<const std::byte>{input}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(RansFrameStreamingEncoder, HandlesEmptyAndEndedCalls) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, marc::frame::stream_header_size> output{};
    marc::frame::RansFrameStreamingEncoder encoder{
        stream_for(0), {}, std::span<std::byte>{unused}.first(0), unused};
    auto result = encoder.process(
        {}, output, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, marc::frame::stream_header_size);
    result = encoder.process({}, {}, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}
} // namespace

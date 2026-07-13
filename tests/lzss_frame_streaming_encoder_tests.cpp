#include "frame/lzss_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 6;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    const auto plan = plan_lzss_stream(stream, {}, {}, input);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzss_stream(stream, {}, {}, input, output).error,
              LzssStreamCodecError::none);
    return output;
}

TEST(LzssFrameStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 6> frame_input{};
    std::array<std::byte, 80> frame_encoded{};
    LzssFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded};
    std::vector<std::byte> actual;
    std::size_t offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(offset,
                                                                    count);
        const auto flags = offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(offset, input.size());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssFrameStreamingEncoder, EmitsFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 6> frame_input{};
    std::array<std::byte, 80> frame_encoded{};
    LzssFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(3), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 3U);
    EXPECT_EQ(first.output_produced, lzss_stream_prefix_size);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(3),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(LzssFrameStreamingEncoder, ReportsWorkspaceLimitAndPrematureEnd) {
    std::array<std::byte, 6> frame_input{};
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 300> output{};
    LzssFrameStreamingEncoder short_encoder{
        config(input.size()), {}, {}, frame_input, short_frame};
    auto result = short_encoder.process(
        std::span<const std::byte>{input}.first(6), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 80> frame_encoded{};
    LzssFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, frame_encoded};
    result = premature.process(
        std::span<const std::byte>{input}.first(3), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(LzssFrameStreamingEncoder, HandlesEmptyPrefixAndEndedCalls) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lzss_stream_prefix_size> output{};
    LzssFrameStreamingEncoder encoder{
        config(0), {}, {}, std::span<std::byte>{unused}.first(0), unused};
    auto result = encoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, lzss_stream_prefix_size);
    result = encoder.process({}, {}, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

} // namespace

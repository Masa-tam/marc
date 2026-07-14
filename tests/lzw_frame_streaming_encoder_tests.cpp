#include "frame/lzw_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    const auto plan = plan_lzw_stream(stream, {}, {}, input, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzw_stream(
                  stream, {}, {}, input, dictionary, output).error,
              LzwStreamCodecError::none);
    return output;
}

TEST(LzwFrameStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 59> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    LzwFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    std::vector<std::byte> actual;
    std::size_t offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            offset, count);
        const auto flags = offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
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

TEST(LzwFrameStreamingEncoder, EmitsFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 59> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    LzwFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(2), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 2U);
    EXPECT_EQ(first.output_produced, lzw_stream_prefix_size);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(2),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(LzwFrameStreamingEncoder, ReportsWorkspaceAndAggregateLimits) {
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 59> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    std::array<std::byte, 2> short_input{};
    LzwFrameStreamingEncoder no_input{
        config(input.size()), {}, {}, short_input, frame_encoded, dictionary};
    EXPECT_EQ(no_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 58> short_encoded{};
    LzwFrameStreamingEncoder no_encoded{
        config(input.size()), {}, {}, frame_input, short_encoded, dictionary};
    std::array<std::byte, 300> output{};
    auto result = no_encoded.process(
        std::span<const std::byte>{input}.first(3), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<marc::dictionary::internal::LzwEncoderEntry, 1>
        short_dictionary{};
    LzwFrameStreamingEncoder no_dictionary{
        config(input.size()), {}, {}, frame_input, frame_encoded,
        short_dictionary};
    EXPECT_EQ(no_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        3 + 59
        + sizeof(marc::dictionary::internal::LzwEncoderEntry) * 2 - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzwFrameStreamingEncoder limited{
        config(input.size()), {}, limits, frame_input, frame_encoded,
        dictionary};
    result = limited.process(
        std::span<const std::byte>{input}.first(3), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzwFrameStreamingEncoder, RejectsPrematureEndAndTrailingInput) {
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 59> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    LzwFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    auto result = premature.process(
        std::span<const std::byte>{input}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    LzwFrameStreamingEncoder trailing{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    result = trailing.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(LzwFrameStreamingEncoder, HandlesEmptyLateEndAndReset) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lzw_stream_prefix_size> output{};
    LzwFrameStreamingEncoder empty{
        config(0), {}, {}, {}, unused, {}};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, lzw_stream_prefix_size);

    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 59> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    LzwFrameStreamingEncoder late_end{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    std::array<std::byte, lzw_stream_prefix_size + 59> encoded{};
    result = late_end.process(
        std::span<const std::byte>{input}.first(3), encoded, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    result = late_end.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    LzwFrameStreamingEncoder reset{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

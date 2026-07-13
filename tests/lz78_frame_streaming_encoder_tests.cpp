#include "frame/lz78_streaming_encoder.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    const auto plan = plan_lz78_stream(stream, {}, {}, input, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz78_stream(
                  stream, {}, {}, input, dictionary, output).error,
              Lz78StreamCodecError::none);
    return output;
}

TEST(Lz78FrameStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 72> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    Lz78FrameStreamingEncoder encoder{
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

TEST(Lz78FrameStreamingEncoder, EmitsFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 72> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    Lz78FrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(2), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 2U);
    EXPECT_EQ(first.output_produced, lz78_stream_prefix_size);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(2),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(Lz78FrameStreamingEncoder, ReportsWorkspaceAndAggregateLimits) {
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 72> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    std::array<std::byte, 2> short_input{};
    Lz78FrameStreamingEncoder no_input{
        config(input.size()), {}, {}, short_input, frame_encoded, dictionary};
    EXPECT_EQ(no_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 71> short_encoded{};
    Lz78FrameStreamingEncoder no_encoded{
        config(input.size()), {}, {}, frame_input, short_encoded, dictionary};
    std::array<std::byte, 300> output{};
    auto result = no_encoded.process(
        std::span<const std::byte>{input}.first(3), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2>
        short_dictionary{};
    Lz78FrameStreamingEncoder no_dictionary{
        config(input.size()), {}, {}, frame_input, frame_encoded,
        short_dictionary};
    EXPECT_EQ(no_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        3 + 72
        + sizeof(marc::dictionary::internal::Lz78EncoderEntry) * 3 - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    Lz78FrameStreamingEncoder limited{
        config(input.size()), {}, limits, frame_input, frame_encoded,
        dictionary};
    result = limited.process(
        std::span<const std::byte>{input}.first(3), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz78FrameStreamingEncoder, RejectsPrematureEndAndTrailingInput) {
    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 72> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    Lz78FrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    auto result = premature.process(
        std::span<const std::byte>{input}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    Lz78FrameStreamingEncoder trailing{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    result = trailing.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(Lz78FrameStreamingEncoder, HandlesEmptyLateEndAndReset) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lz78_stream_prefix_size> output{};
    Lz78FrameStreamingEncoder empty{
        config(0), {}, {}, {}, unused, {}};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, lz78_stream_prefix_size);

    std::array<std::byte, 3> frame_input{};
    std::array<std::byte, 72> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    Lz78FrameStreamingEncoder late_end{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    std::array<std::byte, lz78_stream_prefix_size + 72> encoded{};
    result = late_end.process(
        std::span<const std::byte>{input}.first(3), encoded, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    result = late_end.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    Lz78FrameStreamingEncoder reset{
        config(3), {}, {}, frame_input, frame_encoded, dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

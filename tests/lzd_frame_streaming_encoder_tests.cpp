#include "frame/lzd_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};

StreamHeader config(const std::uint64_t size,
                    const std::uint32_t frame_size = 2) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> reference(
    const std::span<const std::byte> raw = input,
    const std::uint32_t frame_size = 2) {
    const auto stream = config(raw.size(), frame_size);
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw, dictionary, output).error,
              LzdStreamCodecError::none);
    return output;
}

TEST(LzdFrameStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    LzdFrameStreamingEncoder encoder{
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
    EXPECT_EQ(actual.size(), 208U);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzdFrameStreamingEncoder, EmitsFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    LzdFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced, lzd_stream_prefix_size);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(1),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(LzdFrameStreamingEncoder, EncodesFinalShortBinaryFrame) {
    constexpr std::array raw{
        std::byte{0}, std::byte{0xff}, std::byte{0x80}};
    const auto expected = reference(raw, 2);
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    LzdFrameStreamingEncoder encoder{
        config(raw.size()), {}, {}, frame_input, frame_encoded, dictionary};
    std::vector<std::byte> output(expected.size());
    const auto result = encoder.process(
        raw, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    output.resize(result.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(LzdFrameStreamingEncoder, ReportsWorkspaceAndAggregateLimits) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    std::array<std::byte, 1> short_input{};
    LzdFrameStreamingEncoder no_input{
        config(input.size()), {}, {}, short_input, frame_encoded, dictionary};
    EXPECT_EQ(no_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 63> short_encoded{};
    LzdFrameStreamingEncoder no_encoded{
        config(input.size()), {}, {}, frame_input, short_encoded, dictionary};
    std::array<std::byte, 300> output{};
    auto result = no_encoded.process(
        std::span<const std::byte>{input}.first(2), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzdFrameStreamingEncoder no_dictionary{
        config(input.size()), {}, {}, frame_input, frame_encoded, {}};
    EXPECT_EQ(no_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        frame_input.size() + frame_encoded.size()
        + sizeof(marc::dictionary::internal::LzdEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzdFrameStreamingEncoder limited{
        config(input.size()), {}, limits, frame_input, frame_encoded,
        dictionary};
    result = limited.process(
        std::span<const std::byte>{input}.first(2), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzdFrameStreamingEncoder, RejectsPrematureEndAndTrailingInput) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    LzdFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, frame_encoded, dictionary};
    auto result = premature.process(
        std::span<const std::byte>{input}.first(1), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    LzdFrameStreamingEncoder trailing{
        config(2), {}, {}, frame_input, frame_encoded, dictionary};
    result = trailing.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(LzdFrameStreamingEncoder, HandlesEmptyLateEndAndTerminalErrors) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lzd_stream_prefix_size> output{};
    LzdFrameStreamingEncoder empty{
        config(0), {}, {}, {}, unused, {}};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, lzd_stream_prefix_size);

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 64> frame_encoded{};
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    LzdFrameStreamingEncoder late_end{
        config(2), {}, {}, frame_input, frame_encoded, dictionary};
    std::array<std::byte, lzd_stream_prefix_size + 64> encoded{};
    result = late_end.process(
        std::span<const std::byte>{input}.first(2), encoded, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    result = late_end.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    LzdFrameStreamingEncoder reset{
        config(2), {}, {}, frame_input, frame_encoded, dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              marc::core::ErrorCode::unsupported);

    LzdFrameStreamingEncoder unknown{
        config(2), {}, {}, frame_input, frame_encoded, dictionary};
    result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    auto invalid_stream = config(2);
    invalid_stream.dictionary_variant = 2;
    LzdFrameStreamingEncoder invalid{
        invalid_stream, {}, {}, frame_input, frame_encoded, dictionary};
    EXPECT_EQ(invalid.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);
}

} // namespace

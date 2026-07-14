#include "frame/lzw_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> encoded() {
    const auto stream = config();
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    const auto plan = plan_lzw_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzw_stream(
                  stream, {}, {}, raw, dictionary, output).error,
              LzwStreamCodecError::none);
    return output;
}

TEST(LzwFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded();
    std::array<std::byte, 59> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    LzwFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, dictionary};
    std::vector<std::byte> output_bytes;
    std::size_t position{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - position);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            position, count);
        const auto flags = position + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0) output_bytes.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, input.size());
    EXPECT_TRUE(std::ranges::equal(output_bytes, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzwFrameStreamingDecoder, CommitsFirstFrameBeforeLaterCorruption) {
    auto input = encoded();
    constexpr std::size_t second_frame = 139;
    constexpr std::size_t second_payload = second_frame + frame_header_size;
    input[second_payload] = std::byte{0};
    input[second_payload + 1] = std::byte{1};
    std::array<std::byte, 59> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    LzwFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, dictionary};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 3U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(3),
        std::span<const std::byte>{raw}.first(3)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(3),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzwFrameStreamingDecoder, ReportsEachWorkspaceAndTruncation) {
    const auto input = encoded();
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    LzwFrameStreamingDecoder short_encoded{
        {}, short_frame, decoded_frame, dictionary};
    auto result = short_encoded.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 59> frame{};
    std::array<std::byte, 2> short_decoded{};
    LzwFrameStreamingDecoder no_output{
        {}, frame, short_decoded, dictionary};
    result = no_output.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzwFrameStreamingDecoder no_dictionary{
        {}, frame, decoded_frame, {}};
    result = no_dictionary.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzwFrameStreamingDecoder truncated{
        {}, frame, decoded_frame, dictionary};
    std::array<std::byte, raw.size()> partial_output{};
    result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1),
        partial_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(LzwFrameStreamingDecoder, EnforcesAggregateWorkspaceLimit) {
    const auto input = encoded();
    std::array<std::byte, 59> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        59 + 3 + sizeof(marc::dictionary::internal::LzwPhraseEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzwFrameStreamingDecoder decoder{
        limits, frame, decoded_frame, dictionary};
    const auto result = decoder.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzwFrameStreamingDecoder, HandlesEmptyAndRejectsTrailingData) {
    auto stream = config();
    stream.original_size = 0;
    const std::array<std::byte, 0> raw_input{};
    const auto plan = plan_lzw_stream(stream, {}, {}, raw_input, {});
    std::vector<std::byte> input(plan.serialized_size);
    ASSERT_EQ(encode_lzw_stream(
                  stream, {}, {}, raw_input, {}, input).error,
              LzwStreamCodecError::none);
    std::array<std::byte, 1> workspace{};
    LzwFrameStreamingDecoder empty{{}, workspace, workspace, {}};
    auto result = empty.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    input.push_back(std::byte{});
    LzwFrameStreamingDecoder trailing{{}, workspace, workspace, {}};
    result = trailing.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(LzwFrameStreamingDecoder, AcceptsEmptyFinalEndAndRejectsResetBlock) {
    const auto input = encoded();
    std::array<std::byte, 59> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    LzwFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, dictionary};
    std::array<std::byte, raw.size()> output{};
    auto result = decoder.process(input, output, 0);
    ASSERT_EQ(result.status, marc::core::StreamStatus::progress);
    ASSERT_EQ(result.input_consumed, input.size());
    ASSERT_EQ(result.output_produced, raw.size());
    EXPECT_EQ(output, raw);
    result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    LzwFrameStreamingDecoder reset{
        {}, frame, decoded_frame, dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

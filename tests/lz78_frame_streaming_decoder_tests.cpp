#include "frame/lz78_streaming_decoder.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> encoded() {
    const auto stream = config();
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    const auto plan = plan_lz78_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz78_stream(
                  stream, {}, {}, raw, dictionary, output).error,
              Lz78StreamCodecError::none);
    return output;
}

TEST(Lz78FrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded();
    std::array<std::byte, 72> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    Lz78FrameStreamingDecoder decoder{
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

TEST(Lz78FrameStreamingDecoder, CommitsFirstFrameBeforeLaterCorruption) {
    auto input = encoded();
    constexpr std::size_t second_frame = 152;
    constexpr std::size_t second_payload = second_frame + frame_header_size;
    input[second_payload + 8 + 4] = std::byte{2};
    std::array<std::byte, 72> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    Lz78FrameStreamingDecoder decoder{
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

TEST(Lz78FrameStreamingDecoder, ReportsEachWorkspaceAndTruncation) {
    const auto input = encoded();
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    Lz78FrameStreamingDecoder short_encoded{
        {}, short_frame, decoded_frame, dictionary};
    auto result = short_encoded.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 72> frame{};
    std::array<std::byte, 2> short_decoded{};
    Lz78FrameStreamingDecoder no_output{
        {}, frame, short_decoded, dictionary};
    result = no_output.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1>
        short_dictionary{};
    Lz78FrameStreamingDecoder no_dictionary{
        {}, frame, decoded_frame, short_dictionary};
    result = no_dictionary.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    Lz78FrameStreamingDecoder truncated{
        {}, frame, decoded_frame, dictionary};
    std::array<std::byte, raw.size()> partial_output{};
    result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1),
        partial_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(Lz78FrameStreamingDecoder, EnforcesAggregateWorkspaceLimit) {
    const auto input = encoded();
    std::array<std::byte, 72> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        72 + 3
        + sizeof(marc::dictionary::internal::Lz78PhraseEntry) * 2 - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    Lz78FrameStreamingDecoder decoder{
        limits, frame, decoded_frame, dictionary};
    const auto result = decoder.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz78FrameStreamingDecoder, HandlesEmptyAndRejectsTrailingData) {
    auto stream = config();
    stream.original_size = 0;
    const std::array<std::byte, 0> raw_input{};
    const auto plan = plan_lz78_stream(stream, {}, {}, raw_input, {});
    std::vector<std::byte> input(plan.serialized_size);
    ASSERT_EQ(encode_lz78_stream(
                  stream, {}, {}, raw_input, {}, input).error,
              Lz78StreamCodecError::none);
    std::array<std::byte, 1> workspace{};
    Lz78FrameStreamingDecoder empty{{}, workspace, workspace, {}};
    auto result = empty.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    input.push_back(std::byte{});
    Lz78FrameStreamingDecoder trailing{{}, workspace, workspace, {}};
    result = trailing.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(Lz78FrameStreamingDecoder, AcceptsEmptyFinalEndAndRejectsResetBlock) {
    const auto input = encoded();
    std::array<std::byte, 72> frame{};
    std::array<std::byte, 3> decoded_frame{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    Lz78FrameStreamingDecoder decoder{
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

    Lz78FrameStreamingDecoder reset{
        {}, frame, decoded_frame, dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

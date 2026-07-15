#include "frame/lzd_frame_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> encoded() {
    const auto stream = config();
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw, dictionary, output).error,
              LzdStreamCodecError::none);
    return output;
}

TEST(LzdFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded();
    std::array<std::byte, 64> frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzdFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, phrases, expansion};
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

TEST(LzdFrameStreamingDecoder, CommitsFirstFrameBeforeLaterCorruption) {
    auto input = encoded();
    constexpr std::size_t second_payload = 144 + frame_header_size;
    input[second_payload] = std::byte{0};
    input[second_payload + 1] = std::byte{1};
    std::array<std::byte, 64> frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzdFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, phrases, expansion};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 2U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(2),
        std::span<const std::byte>{raw}.first(2)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(2),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzdFrameStreamingDecoder, ReportsEachWorkspaceAndTruncation) {
    const auto input = encoded();
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzdFrameStreamingDecoder short_encoded{
        {}, short_frame, decoded_frame, phrases, expansion};
    auto result = short_encoded.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 64> frame{};
    std::array<std::byte, 1> short_decoded{};
    LzdFrameStreamingDecoder no_output{
        {}, frame, short_decoded, phrases, expansion};
    result = no_output.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzdFrameStreamingDecoder no_phrases{
        {}, frame, decoded_frame, {}, expansion};
    result = no_phrases.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzdFrameStreamingDecoder no_expansion{
        {}, frame, decoded_frame, phrases, {}};
    result = no_expansion.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzdFrameStreamingDecoder truncated{
        {}, frame, decoded_frame, phrases, expansion};
    std::array<std::byte, raw.size()> partial_output{};
    result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1),
        partial_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(LzdFrameStreamingDecoder, EnforcesAggregateWorkspaceLimit) {
    const auto input = encoded();
    std::array<std::byte, 64> frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        frame.size() + decoded_frame.size()
        + sizeof(marc::dictionary::internal::LzdPhraseEntry)
        + expansion.size() * sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzdFrameStreamingDecoder decoder{
        limits, frame, decoded_frame, phrases, expansion};
    const auto result = decoder.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzdFrameStreamingDecoder, HandlesEmptyAndRejectsTrailingData) {
    auto stream = config();
    stream.original_size = 0;
    const std::array<std::byte, 0> raw_input{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw_input, {});
    std::vector<std::byte> input(plan.serialized_size);
    ASSERT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw_input, {}, input).error,
              LzdStreamCodecError::none);
    std::array<std::byte, 1> storage{};
    LzdFrameStreamingDecoder empty{{}, storage, storage, {}, {}};
    auto result = empty.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    input.push_back(std::byte{});
    LzdFrameStreamingDecoder trailing{{}, storage, storage, {}, {}};
    result = trailing.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(LzdFrameStreamingDecoder, AcceptsEmptyFinalEndAndRejectsResetBlock) {
    const auto input = encoded();
    std::array<std::byte, 64> frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzdFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, phrases, expansion};
    std::array<std::byte, raw.size()> output{};
    auto result = decoder.process(input, output, 0);
    ASSERT_EQ(result.status, marc::core::StreamStatus::progress);
    ASSERT_EQ(result.input_consumed, input.size());
    ASSERT_EQ(result.output_produced, raw.size());
    EXPECT_EQ(output, raw);
    result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    LzdFrameStreamingDecoder reset{
        {}, frame, decoded_frame, phrases, expansion};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

TEST(LzdFrameStreamingDecoder, FlushKeepsPartialInputAndErrorsAreTerminal) {
    const auto input = encoded();
    std::array<std::byte, 64> frame{};
    std::array<std::byte, 2> decoded_frame{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzdFrameStreamingDecoder decoder{
        {}, frame, decoded_frame, phrases, expansion};
    auto result = decoder.process(
        std::span<const std::byte>{input}.first(100), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    ASSERT_EQ(result.status, marc::core::StreamStatus::progress);
    ASSERT_EQ(result.input_consumed, 100U);
    std::array<std::byte, raw.size()> output{};
    result = decoder.process(
        std::span<const std::byte>{input}.subspan(100), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(output, raw);

    marc::core::DecoderLimits invalid_limits{};
    invalid_limits.max_frame_size = 0;
    LzdFrameStreamingDecoder invalid{
        invalid_limits, frame, decoded_frame, phrases, expansion};
    result = invalid.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    const auto repeated = invalid.process({}, {}, 0);
    EXPECT_EQ(repeated.error.code, result.error.code);
    EXPECT_EQ(repeated.error.byte_position, result.error.byte_position);

    LzdFrameStreamingDecoder unknown{
        {}, frame, decoded_frame, phrases, expansion};
    result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              marc::core::ErrorCode::unsupported);
}

} // namespace

#include "frame/lzss_blocked_huffman_frame_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
    std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 4;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_stream() {
    const auto stream = config(raw.size());
    std::array<std::byte, 4> staging{};
    const auto plan = plan_lzss_blocked_huffman_stream(
        stream, {}, {}, raw, staging);
    EXPECT_EQ(plan.error, LzssBlockedHuffmanStreamCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzss_blocked_huffman_stream(
                  stream, {}, {}, raw, staging, encoded)
                  .error,
              LzssBlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(LzssBlockedHuffmanFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    LzssBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, encoded.size() - input_offset);
        const auto chunk = std::span<const std::byte>{encoded}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == encoded.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_TRUE(std::ranges::equal(actual, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssBlockedHuffmanFrameStreamingDecoder,
     CommitsOnlyFramesBeforeLaterCorruption) {
    auto encoded = encoded_stream();
    constexpr std::size_t second_frame_payload = 80 + 76 + 56 + 16;
    encoded[second_frame_payload] = std::byte{0xff};
    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    LzssBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    std::array<std::byte, 5> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 2U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{'B'});
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(2),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(LzssBlockedHuffmanFrameStreamingDecoder,
     ReportsIndependentWorkspacesAndAggregate) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 5> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzssBlockedHuffmanFrameStreamingDecoder short_encoded{
        {}, std::span<std::byte>{frame_encoded}.first<75>(),
        dictionary_staging, frame_decoded, views};
    EXPECT_EQ(short_encoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssBlockedHuffmanFrameStreamingDecoder short_dictionary{
        {}, frame_encoded,
        std::span<std::byte>{dictionary_staging}.first<3>(), frame_decoded,
        views};
    EXPECT_EQ(short_dictionary.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssBlockedHuffmanFrameStreamingDecoder short_decoded{
        {}, frame_encoded, dictionary_staging,
        std::span<std::byte>{frame_decoded}.first<1>(), views};
    EXPECT_EQ(short_decoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssBlockedHuffmanFrameStreamingDecoder short_views{
        {}, frame_encoded, dictionary_staging, frame_decoded, {}};
    EXPECT_EQ(short_views.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 4;
    const std::uint64_t required = 76 + 4 + 2
        + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
    limits.max_internal_buffered_bytes = required - 1;
    LzssBlockedHuffmanFrameStreamingDecoder aggregate_limited{
        limits, frame_encoded, dictionary_staging, frame_decoded, views};
    EXPECT_EQ(aggregate_limited.process(encoded, output, end).error.code,
              marc::core::ErrorCode::limit_exceeded);
}

TEST(LzssBlockedHuffmanFrameStreamingDecoder,
     RejectsTruncationTrailingDataAndUnsupportedFlags) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 5> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzssBlockedHuffmanFrameStreamingDecoder truncated{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(
                      encoded.size() - 1), output, end)
                  .error.code,
              marc::core::ErrorCode::malformed_stream);

    auto extended = encoded;
    extended.push_back(std::byte{0});
    LzssBlockedHuffmanFrameStreamingDecoder trailing{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    EXPECT_EQ(trailing.process(extended, output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    LzssBlockedHuffmanFrameStreamingDecoder reset{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    EXPECT_EQ(reset.process(
                  {}, {},
                  marc::core::flag_value(
                      marc::core::ProcessFlags::reset_block))
                  .error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(LzssBlockedHuffmanFrameStreamingDecoder, HandlesEmptyAndFlushStarvation) {
    const auto stream = config(0);
    std::array<std::byte, 1> staging{};
    const auto plan = plan_lzss_blocked_huffman_stream(
        stream, {}, {}, {}, staging);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_blocked_huffman_stream(
                  stream, {}, {}, {}, staging, encoded)
                  .error,
              LzssBlockedHuffmanStreamCodecError::none);
    LzssBlockedHuffmanFrameStreamingDecoder empty{{}, {}, {}, {}, {}};
    const auto result = empty.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    LzssBlockedHuffmanFrameStreamingDecoder starved{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    const auto flush = starved.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(flush.status, marc::core::StreamStatus::need_input);
}

TEST(LzssBlockedHuffmanFrameStreamingDecoder,
     PreservesPrematureEndWhileDrainingNonfinalFrame) {
    const auto encoded = encoded_stream();
    constexpr std::size_t prefix_and_first_frame = 80 + 76;
    std::array<std::byte, 76> frame_encoded{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    LzssBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, dictionary_staging, frame_decoded, views};
    std::array<std::byte, 1> output{};
    auto result = decoder.process(
        std::span<const std::byte>{encoded}.first(prefix_and_first_frame),
        output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::need_output);
    ASSERT_EQ(result.output_produced, 1U);

    result = decoder.process({}, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
}

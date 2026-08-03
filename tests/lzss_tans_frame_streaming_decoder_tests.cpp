#include "frame/lzss_tans_frame_streaming_decoder.hpp"

#include "core/endian.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
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
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 5;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_stream(
    const std::span<const std::byte> input = raw) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    LzssTansFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    std::vector<std::byte> encoded(16384);
    const auto result = encoder.process(
        input, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, input.size());
    encoded.resize(result.output_produced);
    return encoded;
}

[[nodiscard]] std::size_t frame_size_at(
    const std::span<const std::byte> encoded,
    const std::size_t offset) {
    std::uint32_t payload_size{};
    std::uint32_t descriptor_size{};
    EXPECT_TRUE(marc::core::load_le(
        encoded, offset + 24, payload_size));
    EXPECT_TRUE(marc::core::load_le(
        encoded, offset + 32, descriptor_size));
    return frame_header_size + descriptor_size + payload_size;
}

} // namespace

TEST(LzssTansFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    LzssTansFrameStreamingDecoder decoder{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
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
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_TRUE(std::ranges::equal(actual, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssTansFrameStreamingDecoder, CommitsOnlyFramesBeforeLaterCorruption) {
    auto encoded = encoded_stream();
    const auto first_size = frame_size_at(
        encoded, lzss_tans_stream_prefix_size);
    const auto second_offset = lzss_tans_stream_prefix_size + first_size;
    encoded[second_offset + frame_header_size + 17] = std::byte{0x0e};
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    LzssTansFrameStreamingDecoder decoder{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    std::array<std::byte, raw.size()> output{};
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
        [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(LzssTansFrameStreamingDecoder, ReportsWorkspaceAndAggregateErrors) {
    const auto encoded = encoded_stream();
    const auto first_size = frame_size_at(
        encoded, lzss_tans_stream_prefix_size);
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzssTansFrameStreamingDecoder short_encoded{
        {}, std::span<std::byte>{frame_encoded}.first(first_size - 1),
        views, dictionary_staging, frame_decoded};
    EXPECT_EQ(short_encoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssTansFrameStreamingDecoder short_views{
        {}, frame_encoded,
        std::span<marc::entropy::internal::TansBlockView>{},
        dictionary_staging, frame_decoded};
    EXPECT_EQ(short_views.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssTansFrameStreamingDecoder short_dictionary{
        {}, frame_encoded, views,
        std::span<std::byte>{dictionary_staging}.first<3>(),
        frame_decoded};
    EXPECT_EQ(short_dictionary.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssTansFrameStreamingDecoder short_decoded{
        {}, frame_encoded, views, dictionary_staging,
        std::span<std::byte>{frame_decoded}.first<1>()};
    EXPECT_EQ(short_decoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 5;
    limits.max_internal_buffered_bytes =
        first_size + 4 + 2
        + sizeof(marc::entropy::internal::TansBlockView) - 1;
    LzssTansFrameStreamingDecoder aggregate_limited{
        limits, frame_encoded, views, dictionary_staging, frame_decoded};
    EXPECT_EQ(aggregate_limited.process(
                  encoded, output, end).error.code,
              marc::core::ErrorCode::limit_exceeded);
}

TEST(LzssTansFrameStreamingDecoder, RejectsTruncationTrailingAndReset) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzssTansFrameStreamingDecoder truncated{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(
                      encoded.size() - 1),
                  output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    auto extended = encoded;
    extended.push_back(std::byte{0});
    LzssTansFrameStreamingDecoder trailing{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    EXPECT_EQ(trailing.process(extended, output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    LzssTansFrameStreamingDecoder reset{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(
                              marc::core::ProcessFlags::reset_block))
                  .error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(LzssTansFrameStreamingDecoder, HandlesEmptyFlushAndPrematureEnd) {
    const auto empty_encoded = encoded_stream({});
    LzssTansFrameStreamingDecoder empty{{}, {}, {}, {}, {}};
    EXPECT_EQ(empty.process(
                  empty_encoded, {},
                  marc::core::flag_value(marc::core::ProcessFlags::end_input))
                  .status,
              marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 1> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    LzssTansFrameStreamingDecoder starved{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    EXPECT_EQ(starved.process(
                  {}, {}, marc::core::flag_value(
                              marc::core::ProcessFlags::flush))
                  .status,
              marc::core::StreamStatus::need_input);

    const auto encoded = encoded_stream();
    const auto first_size = frame_size_at(
        encoded, lzss_tans_stream_prefix_size);
    const auto prefix_and_first_frame =
        lzss_tans_stream_prefix_size + first_size;
    LzssTansFrameStreamingDecoder premature{
        {}, frame_encoded, views, dictionary_staging, frame_decoded};
    std::array<std::byte, 1> output{};
    auto result = premature.process(
        std::span<const std::byte>{encoded}.first(prefix_and_first_frame),
        output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::need_output);
    ASSERT_EQ(result.output_produced, 1U);
    result = premature.process({}, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
}


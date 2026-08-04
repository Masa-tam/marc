#include "frame/lzw_tans_frame_streaming_decoder.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_stream(
    const std::span<const std::byte> input = raw) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry, 1> entries{};
    LzwTansFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded, entries};
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
    EXPECT_TRUE(marc::core::load_le(encoded, offset + 24, payload_size));
    EXPECT_TRUE(marc::core::load_le(encoded, offset + 32, descriptor_size));
    return frame_header_size + descriptor_size + payload_size;
}

struct Workspace {
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::entropy::internal::TansBlockView, 4> views{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 8> phrases{};
};

} // namespace

TEST(LzwTansFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto encoded = encoded_stream();
    Workspace workspace{};
    LzwTansFrameStreamingDecoder decoder{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count =
            std::min<std::size_t>(1, encoded.size() - input_offset);
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

TEST(LzwTansFrameStreamingDecoder, CommitsOnlyFramesBeforeLaterCorruption) {
    auto encoded = encoded_stream();
    const auto first_size =
        frame_size_at(encoded, lzw_tans_stream_prefix_size);
    const auto second_offset = lzw_tans_stream_prefix_size + first_size;
    encoded[second_offset + frame_header_size + 17] = std::byte{0x0e};
    Workspace workspace{};
    LzwTansFrameStreamingDecoder decoder{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
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
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(LzwTansFrameStreamingDecoder, ReportsWorkspaceAndAggregateErrors) {
    const auto encoded = encoded_stream();
    const auto first_offset = lzw_tans_stream_prefix_size;
    const auto first_size = frame_size_at(encoded, first_offset);
    std::uint32_t dictionary_size{};
    std::uint32_t block_count{};
    ASSERT_TRUE(marc::core::load_le(
        encoded, first_offset + 20, dictionary_size));
    ASSERT_TRUE(marc::core::load_le(
        encoded, first_offset + 28, block_count));
    const auto phrase_count =
        marc::dictionary::internal::lzw_validation_workspace_entries(
            dictionary_size, {});
    Workspace workspace{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzwTansFrameStreamingDecoder short_encoded{
        {},
        std::span<std::byte>{workspace.frame_encoded}.first(first_size - 1),
        workspace.views, workspace.dictionary_staging,
        workspace.frame_decoded, workspace.phrases};
    EXPECT_EQ(short_encoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzwTansFrameStreamingDecoder short_views{
        {}, workspace.frame_encoded,
        std::span<marc::entropy::internal::TansBlockView>{workspace.views}
            .first(block_count - 1),
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(short_views.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzwTansFrameStreamingDecoder short_dictionary{
        {}, workspace.frame_encoded, workspace.views,
        std::span<std::byte>{workspace.dictionary_staging}.first(
            dictionary_size - 1),
        workspace.frame_decoded, workspace.phrases};
    EXPECT_EQ(short_dictionary.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzwTansFrameStreamingDecoder short_decoded{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging,
        std::span<std::byte>{workspace.frame_decoded}.first<1>(),
        workspace.phrases};
    EXPECT_EQ(short_decoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzwTansFrameStreamingDecoder short_phrases{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        std::span<marc::dictionary::internal::LzwPhraseEntry>{workspace.phrases}
            .first(phrase_count - 1)};
    EXPECT_EQ(short_phrases.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 2;
    limits.max_internal_buffered_bytes =
        first_size + dictionary_size + 2
        + block_count * sizeof(marc::entropy::internal::TansBlockView)
        + phrase_count * sizeof(marc::dictionary::internal::LzwPhraseEntry)
        - 1;
    LzwTansFrameStreamingDecoder aggregate_limited{
        limits, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(aggregate_limited.process(encoded, output, end).error.code,
              marc::core::ErrorCode::limit_exceeded);
}

TEST(LzwTansFrameStreamingDecoder, RejectsTruncationTrailingAndReset) {
    const auto encoded = encoded_stream();
    Workspace workspace{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzwTansFrameStreamingDecoder truncated{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(
        truncated
            .process(std::span<const std::byte>{encoded}.first(
                         encoded.size() - 1),
                     output, end)
            .error.code,
        marc::core::ErrorCode::malformed_stream);

    auto extended = encoded;
    extended.push_back(std::byte{0});
    LzwTansFrameStreamingDecoder trailing{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(trailing.process(extended, output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    LzwTansFrameStreamingDecoder reset{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(
        reset
            .process({}, {},
                     marc::core::flag_value(
                         marc::core::ProcessFlags::reset_block))
            .error.code,
        marc::core::ErrorCode::unsupported);

    LzwTansFrameStreamingDecoder unknown{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(LzwTansFrameStreamingDecoder, HandlesEmptyFlushAndPrematureEnd) {
    const auto empty_encoded = encoded_stream({});
    LzwTansFrameStreamingDecoder empty{{}, {}, {}, {}, {}, {}};
    EXPECT_EQ(
        empty
            .process(empty_encoded, {},
                     marc::core::flag_value(
                         marc::core::ProcessFlags::end_input))
            .status,
        marc::core::StreamStatus::end_of_stream);

    Workspace workspace{};
    LzwTansFrameStreamingDecoder starved{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
    EXPECT_EQ(
        starved
            .process({}, {},
                     marc::core::flag_value(marc::core::ProcessFlags::flush))
            .status,
        marc::core::StreamStatus::need_input);

    const auto encoded = encoded_stream();
    const auto first_size =
        frame_size_at(encoded, lzw_tans_stream_prefix_size);
    const auto prefix_and_first_frame =
        lzw_tans_stream_prefix_size + first_size;
    LzwTansFrameStreamingDecoder premature{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases};
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

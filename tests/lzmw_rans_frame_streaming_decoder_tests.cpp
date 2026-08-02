#include "frame/lzmw_rans_frame_streaming_decoder.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_stream(
    const std::span<const std::byte> input = raw) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 8> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    LzmwRansFrameStreamingEncoder encoder{
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
    std::array<marc::entropy::internal::RansBlockView, 4> views{};
    std::array<std::byte, 8> dictionary_staging{};
    std::array<std::byte, 2> frame_decoded{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
};

} // namespace

TEST(LzmwRansFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto encoded = encoded_stream();
    Workspace workspace{};
    LzmwRansFrameStreamingDecoder decoder{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
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
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_TRUE(std::ranges::equal(actual, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwRansFrameStreamingDecoder, CommitsOnlyFramesBeforeLaterCorruption) {
    auto encoded = encoded_stream();
    const auto first_size =
        frame_size_at(encoded, lzmw_rans_stream_prefix_size);
    const auto second_offset = lzmw_rans_stream_prefix_size + first_size;
    encoded[second_offset + frame_header_size + 17] = std::byte{0x0e};
    Workspace workspace{};
    LzmwRansFrameStreamingDecoder decoder{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
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

TEST(LzmwRansFrameStreamingDecoder, ReportsWorkspaceAndAggregateErrors) {
    const auto encoded = encoded_stream();
    const auto first_offset = lzmw_rans_stream_prefix_size;
    const auto first_size = frame_size_at(encoded, first_offset);
    std::uint32_t raw_size{};
    std::uint32_t dictionary_size{};
    std::uint32_t block_count{};
    ASSERT_TRUE(marc::core::load_le(encoded, first_offset + 16, raw_size));
    ASSERT_TRUE(marc::core::load_le(
        encoded, first_offset + 20, dictionary_size));
    ASSERT_TRUE(marc::core::load_le(
        encoded, first_offset + 28, block_count));
    const auto phrase_count =
        marc::dictionary::internal::lzmw_validation_workspace_entries(
            dictionary_size, {});
    const auto expansion_count =
        marc::dictionary::internal::lzmw_expansion_workspace_entries(
            phrase_count, raw_size != 0);
    Workspace workspace{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzmwRansFrameStreamingDecoder short_encoded{
        {},
        std::span<std::byte>{workspace.frame_encoded}.first(first_size - 1),
        workspace.views, workspace.dictionary_staging,
        workspace.frame_decoded, workspace.phrases, workspace.expansion};
    EXPECT_EQ(short_encoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwRansFrameStreamingDecoder short_views{
        {}, workspace.frame_encoded,
        std::span<marc::entropy::internal::RansBlockView>{workspace.views}
            .first(block_count - 1),
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(short_views.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwRansFrameStreamingDecoder short_dictionary{
        {}, workspace.frame_encoded, workspace.views,
        std::span<std::byte>{workspace.dictionary_staging}.first(
            dictionary_size - 1),
        workspace.frame_decoded, workspace.phrases, workspace.expansion};
    EXPECT_EQ(short_dictionary.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwRansFrameStreamingDecoder short_decoded{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging,
        std::span<std::byte>{workspace.frame_decoded}.first(raw_size - 1),
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(short_decoded.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwRansFrameStreamingDecoder short_phrases{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        std::span<marc::dictionary::internal::LzmwPhraseEntry>{
            workspace.phrases}.first(phrase_count - 1),
        workspace.expansion};
    EXPECT_EQ(short_phrases.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwRansFrameStreamingDecoder short_expansion{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases,
        std::span<std::uint32_t>{workspace.expansion}.first(
            expansion_count - 1)};
    EXPECT_EQ(short_expansion.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 2;
    limits.max_internal_buffered_bytes =
        first_size + dictionary_size + raw_size
        + block_count * sizeof(marc::entropy::internal::RansBlockView)
        + phrase_count * sizeof(marc::dictionary::internal::LzmwPhraseEntry)
        + expansion_count * sizeof(std::uint32_t) - 1;
    LzmwRansFrameStreamingDecoder aggregate_limited{
        limits, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(aggregate_limited.process(encoded, output, end).error.code,
              marc::core::ErrorCode::limit_exceeded);
}

TEST(LzmwRansFrameStreamingDecoder, RejectsTruncationTrailingAndReset) {
    const auto encoded = encoded_stream();
    Workspace workspace{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzmwRansFrameStreamingDecoder truncated{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(
                      encoded.size() - 1),
                  output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    auto extended = encoded;
    extended.push_back(std::byte{0});
    LzmwRansFrameStreamingDecoder trailing{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(trailing.process(extended, output, end).error.code,
              marc::core::ErrorCode::malformed_stream);

    LzmwRansFrameStreamingDecoder reset{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(
                              marc::core::ProcessFlags::reset_block))
                  .error.code,
              marc::core::ErrorCode::unsupported);

    LzmwRansFrameStreamingDecoder unknown{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(LzmwRansFrameStreamingDecoder, HandlesEmptyFlushAndPrematureEnd) {
    const auto empty_encoded = encoded_stream({});
    LzmwRansFrameStreamingDecoder empty{{}, {}, {}, {}, {}, {}, {}};
    EXPECT_EQ(empty.process(
                  empty_encoded, {}, marc::core::flag_value(
                                         marc::core::ProcessFlags::end_input))
                  .status,
              marc::core::StreamStatus::end_of_stream);

    Workspace workspace{};
    LzmwRansFrameStreamingDecoder starved{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
    EXPECT_EQ(starved.process(
                  {}, {}, marc::core::flag_value(
                              marc::core::ProcessFlags::flush))
                  .status,
              marc::core::StreamStatus::need_input);

    const auto encoded = encoded_stream();
    const auto first_size =
        frame_size_at(encoded, lzmw_rans_stream_prefix_size);
    const auto prefix_and_first_frame =
        lzmw_rans_stream_prefix_size + first_size;
    LzmwRansFrameStreamingDecoder premature{
        {}, workspace.frame_encoded, workspace.views,
        workspace.dictionary_staging, workspace.frame_decoded,
        workspace.phrases, workspace.expansion};
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

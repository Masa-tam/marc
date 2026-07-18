#include "frame/lzmw_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzmw_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array raw{std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
                         std::byte{'B'}, std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 4;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage, const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

[[nodiscard]] std::vector<std::byte> encoded_stream() {
    const auto stream = config(raw.size());
    std::vector<std::byte> encoded(lzmw_blocked_huffman_stream_prefix_size);
    const std::span<std::byte, stream_header_size> header{
        encoded.data(), stream_header_size};
    const std::span<std::byte, marc::dictionary::internal::lzmw_parameter_size>
        parameters{encoded.data() + stream_header_size,
                   marc::dictionary::internal::lzmw_parameter_size};
    EXPECT_EQ(serialize_stream_header(stream, {}, header),
              StreamHeaderError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lzmw_parameters(
                  {}, {}, parameters),
              marc::dictionary::internal::LzmwFormatError::none);

    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    std::array<std::byte, 8> staging{};
    std::uint64_t sequence{};
    for (std::size_t offset = 0; offset < raw.size(); offset += 2) {
        const auto input = std::span<const std::byte>{raw}.subspan(
            offset, std::min<std::size_t>(2, raw.size() - offset));
        const auto plan = plan_lzmw_blocked_huffman_frame(
            stream, {}, {}, sequence, offset, input, entries, staging);
        EXPECT_EQ(plan.error, LzmwBlockedHuffmanFrameValidationError::none);
        const auto original_size = encoded.size();
        encoded.resize(original_size + plan.serialized_size);
        EXPECT_EQ(encode_lzmw_blocked_huffman_frame(
                      stream, {}, {}, sequence, offset, input, entries,
                      staging,
                      std::span<std::byte>{encoded}.subspan(original_size))
                      .error,
                  LzmwBlockedHuffmanFrameValidationError::none);
        ++sequence;
    }
    return encoded;
}

} // namespace

TEST(LzmwBlockedHuffmanFrameStreaming, ProfileWorkspacesAreDirectlyUsable) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = raw.size();
    limits.max_frame_size = 2;
    limits.max_block_size = 4;
    limits.max_compressed_payload_size = 8;
    limits.max_dictionary_serialized_size = 8;
    limits.max_dictionary_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    limits.max_blocks_per_frame = 2;
    limits.max_internal_buffered_bytes = 512;

    StreamHeader stream{};
    LzmwBlockedHuffmanEncoderWorkspaceRequirements encoder_requirements{};
    const LzmwBlockedHuffmanProfileConfig profile_config{
        raw.size(), 2, 4, {}};
    ASSERT_EQ(make_lzmw_blocked_huffman_profile(
                  profile_config, limits, stream, encoder_requirements),
              LzmwBlockedHuffmanProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> staging(
        encoder_requirements.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> opaque_allocation(
        encoder_requirements.views_bytes
        + encoder_requirements.views_alignment);
    auto opaque = aligned_storage(
        opaque_allocation, encoder_requirements.views_bytes,
        encoder_requirements.views_alignment);
    LzmwBlockedHuffmanEncoderViews encoder_views{};
    ASSERT_EQ(partition_lzmw_blocked_huffman_encoder_views(
                  encoder_requirements, opaque, encoder_views),
              LzmwBlockedHuffmanWorkspaceError::none);
    LzmwBlockedHuffmanFrameStreamingEncoder encoder{
        stream, profile_config.parameters, limits, frame_input, staging,
        frame_encoded, encoder_views.entries};
    EXPECT_NE(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::error);

    LzmwBlockedHuffmanDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzmw_blocked_huffman_decoder_workspace(
                  limits, decoder_requirements),
              LzmwBlockedHuffmanProfileError::none);
    std::vector<std::byte> decoder_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> decoder_staging(
        decoder_requirements.dictionary_staging_bytes);
    std::vector<std::byte> decoder_output(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::byte> decoder_opaque_allocation(
        decoder_requirements.views_bytes
        + decoder_requirements.views_alignment);
    auto decoder_opaque = aligned_storage(
        decoder_opaque_allocation, decoder_requirements.views_bytes,
        decoder_requirements.views_alignment);
    LzmwBlockedHuffmanDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzmw_blocked_huffman_decoder_views(
                  decoder_requirements, decoder_opaque, decoder_views),
              LzmwBlockedHuffmanWorkspaceError::none);
    LzmwBlockedHuffmanFrameStreamingDecoder decoder{
        limits, decoder_encoded, decoder_staging, decoder_output,
        decoder_views.blocks, decoder_views.phrases,
        decoder_views.expansion};
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::need_input);
}

TEST(LzmwBlockedHuffmanFrameStreaming, OneByteEncoderMatchesFrameOracle) {
    const auto expected = encoded_stream();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 96> frame_encoded{};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    LzmwBlockedHuffmanFrameStreamingEncoder encoder{
        config(raw.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, raw.size() - input_offset);
        const auto chunk = std::span<const std::byte>{raw}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == raw.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwBlockedHuffmanFrameStreaming, DecoderHandlesOneByteBoundaries) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 96> frame_encoded{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzmwBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, staging, decoded, views, phrases, expansion};
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
    EXPECT_TRUE(std::ranges::equal(actual, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwBlockedHuffmanFrameStreaming,
     LaterCorruptionIsStickyAndFrameAtomic) {
    auto encoded = encoded_stream();
    constexpr std::size_t second_frame_payload = 80 + 96 + 56 + 32;
    encoded[second_frame_payload] = std::byte{0};
    encoded[second_frame_payload + 1] = std::byte{1};
    std::array<std::byte, 96> frame_encoded{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    LzmwBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, staging, decoded, views, phrases, expansion};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 2U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{'B'});
    EXPECT_EQ(output[2], std::byte{0x5a});
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(LzmwBlockedHuffmanFrameStreaming,
     RejectsWorkspacesTruncationAndReset) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 96> frame_encoded{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw.size()> output{};
    const auto end =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    LzmwBlockedHuffmanFrameStreamingDecoder short_phrases{
        {}, frame_encoded, staging, decoded, views, {}, expansion};
    EXPECT_EQ(short_phrases.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);
    LzmwBlockedHuffmanFrameStreamingDecoder short_expansion{
        {}, frame_encoded, staging, decoded, views, phrases, {}};
    EXPECT_EQ(short_expansion.process(encoded, output, end).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzmwBlockedHuffmanFrameStreamingDecoder truncated{
        {}, frame_encoded, staging, decoded, views, phrases, expansion};
    EXPECT_EQ(truncated
                  .process(std::span<const std::byte>{encoded}.first(
                               encoded.size() - 1),
                           output, end)
                  .error.code,
              marc::core::ErrorCode::malformed_stream);

    LzmwBlockedHuffmanFrameStreamingDecoder reset{
        {}, frame_encoded, staging, decoded, views, phrases, expansion};
    EXPECT_EQ(reset
                  .process({}, {}, marc::core::flag_value(
                                       marc::core::ProcessFlags::reset_block))
                  .error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(LzmwBlockedHuffmanFrameStreaming, EmptyStreamAndPrematureEndAreExact) {
    std::array<std::byte, lzmw_blocked_huffman_stream_prefix_size> prefix{};
    std::array<std::byte, 1> unused{};
    LzmwBlockedHuffmanFrameStreamingEncoder empty_encoder{
        config(0), {}, {}, {}, {}, unused, {}};
    const auto encoded = empty_encoder.process(
        {}, prefix,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(encoded.output_produced, prefix.size());

    LzmwBlockedHuffmanFrameStreamingDecoder empty_decoder{
        {}, {}, {}, {}, {}, {}, {}};
    EXPECT_EQ(empty_decoder
                  .process(prefix, {}, marc::core::flag_value(
                                           marc::core::ProcessFlags::end_input))
                  .status,
              marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 96> frame_encoded{};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    LzmwBlockedHuffmanFrameStreamingEncoder premature{
        config(raw.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    EXPECT_EQ(premature
                  .process(std::span<const std::byte>{raw}.first<1>(), {},
                           marc::core::flag_value(
                               marc::core::ProcessFlags::end_input))
                  .error.code,
              marc::core::ErrorCode::invalid_argument);
}

TEST(LzmwBlockedHuffmanFrameStreaming, FlushPreservesPartialInput) {
    const auto expected = encoded_stream();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 96> frame_encoded{};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    LzmwBlockedHuffmanFrameStreamingEncoder encoder{
        config(raw.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    std::array<std::byte, lzmw_blocked_huffman_stream_prefix_size> prefix{};
    EXPECT_EQ(encoder.process({}, prefix, 0).status,
              marc::core::StreamStatus::progress);
    const auto partial = encoder.process(
        std::span<const std::byte>{raw}.first<1>(), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(partial.input_consumed, 1U);
    EXPECT_EQ(partial.status, marc::core::StreamStatus::progress);
    std::array<std::byte, 3 * 96> frames{};
    const auto finished = encoder.process(
        std::span<const std::byte>{raw}.subspan(1), frames,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(finished.status, marc::core::StreamStatus::end_of_stream);
    std::vector<std::byte> combined(prefix.begin(), prefix.end());
    combined.insert(combined.end(), frames.begin(),
                    frames.begin() + finished.output_produced);
    EXPECT_EQ(combined, expected);
}

TEST(LzmwBlockedHuffmanFrameStreaming, EnforcesFrameAggregateLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = raw.size();
    limits.max_frame_size = 2;
    limits.max_block_size = 4;
    limits.max_compressed_payload_size = 8;
    limits.max_dictionary_serialized_size = 8;
    limits.max_dictionary_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    limits.max_blocks_per_frame = 2;

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 8> staging{};
    std::array<std::byte, 96> frame_encoded{};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> entries{};
    limits.max_internal_buffered_bytes = frame_input.size() + staging.size()
        + frame_encoded.size() + sizeof(entries) - 1;
    LzmwBlockedHuffmanFrameStreamingEncoder encoder{
        config(raw.size()), {}, limits, frame_input, staging, frame_encoded,
        entries};
    std::array<std::byte, 512> encoded_output{};
    EXPECT_EQ(encoder
                  .process(raw, encoded_output,
                           marc::core::flag_value(
                               marc::core::ProcessFlags::end_input))
                  .error.code,
              marc::core::ErrorCode::limit_exceeded);

    const auto encoded = encoded_stream();
    std::array<std::byte, 2> decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 2> views{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    limits.max_internal_buffered_bytes = frame_encoded.size() + staging.size()
        + decoded.size() + sizeof(views) + sizeof(phrases)
        + sizeof(expansion) - 1;
    LzmwBlockedHuffmanFrameStreamingDecoder decoder{
        limits, frame_encoded, staging, decoded, views, phrases, expansion};
    std::array<std::byte, raw.size()> raw_output{};
    EXPECT_EQ(decoder
                  .process(encoded, raw_output,
                           marc::core::flag_value(
                               marc::core::ProcessFlags::end_input))
                  .error.code,
              marc::core::ErrorCode::limit_exceeded);
}

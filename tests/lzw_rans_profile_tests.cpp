#include "frame/lzw_rans_profile.hpp"
#include "frame/lzw_rans_frame_streaming_decoder.hpp"
#include "frame/lzw_rans_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage,
    const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzwRansProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    StreamHeader stream{};
    LzwRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzw_rans_profile(
                  {17, 10, 4, {}}, {}, stream, workspace),
              LzwRansProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzw);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::rans);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(stream.entropy_block_size, 4U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 20U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2'756U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(
        workspace.views_bytes,
        9U * sizeof(marc::dictionary::internal::LzwEncoderEntry));
    EXPECT_EQ(
        workspace.views_alignment,
        alignof(marc::dictionary::internal::LzwEncoderEntry));
}

TEST(LzwRansProfile, UsesShortFrameAndCanonicalEmptyLayout) {
    StreamHeader stream{};
    LzwRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzw_rans_profile(
                  {7, 16, 8, {}}, {}, stream, workspace),
              LzwRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 14U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 1'142U);
    EXPECT_EQ(workspace.encoder_entry_count, 6U);

    workspace = {1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzw_rans_profile({}, {}, stream, workspace),
              LzwRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzwRansProfile, EnforcesBlockPayloadAggregateAndFrameLimits) {
    StreamHeader stream{};
    LzwRansEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(make_lzw_rans_profile(
                  {2, 2, 2, {}}, limits, stream, workspace),
              LzwRansProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 41;
    EXPECT_EQ(make_lzw_rans_profile(
                  {17}, limits, stream, workspace),
              LzwRansProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 2;
    limits.max_internal_buffered_bytes = 596;
    EXPECT_EQ(make_lzw_rans_profile(
                  {1, 1, 2, {}}, limits, stream, workspace),
              LzwRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);

    limits = {};
    limits.max_frame_size = 2U << 20;
    EXPECT_EQ(make_lzw_rans_profile(
                  {(1U << 20) + 1U, (1U << 20) + 1U, 65'536, {}},
                  limits, stream, workspace),
              LzwRansProfileError::limit_exceeded);

    LzwRansProfileConfig invalid{};
    invalid.parameters.maximum_code_width = 8;
    EXPECT_EQ(make_lzw_rans_profile(invalid, {}, stream, workspace),
              LzwRansProfileError::invalid_configuration);
}

TEST(LzwRansProfile, CalculatesDecoderLayoutFromLocalLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 300;
    LzwRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzw_rans_decoder_workspace(limits, workspace),
              LzwRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.block_view_count, 4U);
    EXPECT_EQ(workspace.phrase_entry_count, 112U);
    EXPECT_EQ(
        workspace.phrase_offset
            % alignof(marc::dictionary::internal::LzwPhraseEntry),
        0U);
    EXPECT_EQ(
        workspace.views_alignment,
        std::max(alignof(marc::entropy::internal::RansBlockView),
                 alignof(marc::dictionary::internal::LzwPhraseEntry)));
    EXPECT_EQ(
        workspace.views_bytes,
        workspace.phrase_offset
            + workspace.phrase_entry_count
                * sizeof(marc::dictionary::internal::LzwPhraseEntry));

    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(calculate_lzw_rans_decoder_workspace(limits, workspace),
              LzwRansProfileError::arithmetic_overflow);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzwRansProfile, PartitionsEncoderAndDecoderOpaqueStorage) {
    StreamHeader stream{};
    LzwRansEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(make_lzw_rans_profile(
                  {4, 4, 4, {}}, {}, stream, encoder),
              LzwRansProfileError::none);
    std::vector<std::byte> encoder_allocation(
        encoder.views_bytes + encoder.views_alignment);
    auto encoder_storage = aligned_storage(
        encoder_allocation, encoder.views_bytes, encoder.views_alignment);
    LzwRansEncoderViews encoder_views{};
    ASSERT_EQ(partition_lzw_rans_encoder_views(
                  encoder, encoder_storage, encoder_views),
              LzwRansWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), encoder.encoder_entry_count);
    EXPECT_EQ(partition_lzw_rans_encoder_views(
                  encoder,
                  encoder_storage.first(encoder_storage.size() - 1),
                  encoder_views),
              LzwRansWorkspaceError::too_small);
    auto invalid_encoder = encoder;
    ++invalid_encoder.encoder_entry_count;
    EXPECT_EQ(partition_lzw_rans_encoder_views(
                  invalid_encoder, encoder_storage, encoder_views),
              LzwRansWorkspaceError::invalid_requirements);
    if (encoder.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{
            encoder_storage.data() + 1, encoder_storage.size()};
        EXPECT_EQ(partition_lzw_rans_encoder_views(
                      encoder, misaligned, encoder_views),
                  LzwRansWorkspaceError::misaligned);
    }

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 300;
    LzwRansDecoderWorkspaceRequirements decoder{};
    ASSERT_EQ(calculate_lzw_rans_decoder_workspace(limits, decoder),
              LzwRansProfileError::none);
    std::vector<std::byte> decoder_allocation(
        decoder.views_bytes + decoder.views_alignment);
    auto decoder_storage = aligned_storage(
        decoder_allocation, decoder.views_bytes, decoder.views_alignment);
    LzwRansDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzw_rans_decoder_views(
                  decoder, decoder_storage, decoder_views),
              LzwRansWorkspaceError::none);
    EXPECT_EQ(decoder_views.blocks.size(), decoder.block_view_count);
    EXPECT_EQ(decoder_views.phrases.size(), decoder.phrase_entry_count);
    EXPECT_EQ(
        reinterpret_cast<std::byte*>(decoder_views.phrases.data()),
        decoder_storage.data() + decoder.phrase_offset);

    auto invalid = decoder;
    ++invalid.phrase_offset;
    EXPECT_EQ(partition_lzw_rans_decoder_views(
                  invalid, decoder_storage, decoder_views),
              LzwRansWorkspaceError::invalid_requirements);
    if (decoder.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{
            decoder_storage.data() + 1, decoder_storage.size()};
        EXPECT_EQ(partition_lzw_rans_decoder_views(
                      decoder, misaligned, decoder_views),
                  LzwRansWorkspaceError::misaligned);
    }
}

TEST(LzwRansProfile, MapsStableCoreErrors) {
    EXPECT_EQ(lzw_rans_profile_error_code(LzwRansProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lzw_rans_profile_error_code(
                  LzwRansProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lzw_rans_profile_error_code(LzwRansProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lzw_rans_profile_error_code(
                  LzwRansProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(lzw_rans_profile_error_code(
                  LzwRansProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);

    auto limits = marc::core::DecoderLimits{};
    limits.max_dictionary_entries = 253;
    LzwRansDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzw_rans_decoder_workspace(limits, workspace),
              LzwRansProfileError::limit_exceeded);
}

TEST(LzwRansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 16'384;
    limits.max_blocks_per_frame = 7;
    const LzwRansProfileConfig config{raw.size(), 2, 2, {}};
    StreamHeader stream{};
    LzwRansEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(make_lzw_rans_profile(
                  config, limits, stream, encoder_ws),
              LzwRansProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    std::vector<std::byte> encode_views_allocation(
        encoder_ws.views_bytes + encoder_ws.views_alignment);
    auto encode_views_storage = aligned_storage(
        encode_views_allocation, encoder_ws.views_bytes,
        encoder_ws.views_alignment);
    LzwRansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzw_rans_encoder_views(
                  encoder_ws, encode_views_storage, encode_views),
              LzwRansWorkspaceError::none);
    LzwRansFrameStreamingEncoder encoder{
        stream, config.parameters, limits, frame_input, encode_dictionary,
        frame_encoded, encode_views.entries};
    std::vector<std::byte> encoded(16'384);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzwRansDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(calculate_lzw_rans_decoder_workspace(limits, decoder_ws),
              LzwRansProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_decoded(decoder_ws.frame_decoded_bytes);
    std::vector<std::byte> decode_views_allocation(
        decoder_ws.views_bytes + decoder_ws.views_alignment);
    auto decode_views_storage = aligned_storage(
        decode_views_allocation, decoder_ws.views_bytes,
        decoder_ws.views_alignment);
    LzwRansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzw_rans_decoder_views(
                  decoder_ws, decode_views_storage, decode_views),
              LzwRansWorkspaceError::none);
    LzwRansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.blocks, decode_dictionary,
        frame_decoded, decode_views.phrases};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

} // namespace

#include "frame/lzmw_rans_profile.hpp"
#include "frame/lzmw_rans_frame_streaming_decoder.hpp"
#include "frame/lzmw_rans_frame_streaming_encoder.hpp"

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

TEST(LzmwRansProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    StreamHeader stream{};
    LzmwRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzmw_rans_profile(
                  {17, 10, 4, {}}, {}, stream, workspace),
              LzmwRansProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzmw);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::rans);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(stream.entropy_block_size, 4U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 5'456U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzmwEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzmwEncoderEntry));
}

TEST(LzmwRansProfile, HonorsFreezeShortFrameAndEmptyStream) {
    StreamHeader stream{};
    LzmwRansEncoderWorkspaceRequirements workspace{};
    LzmwRansProfileConfig config{7, 16, 8, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(make_lzmw_rans_profile(config, {}, stream, workspace),
              LzmwRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 28U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2'228U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    ASSERT_EQ(make_lzmw_rans_profile({}, {}, stream, workspace),
              LzmwRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzmwRansProfile, EnforcesBlockPayloadAggregateAndFrameLimits) {
    StreamHeader stream{};
    LzmwRansEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(make_lzmw_rans_profile(
                  {2, 2, 2, {}}, limits, stream, workspace),
              LzmwRansProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 15;
    EXPECT_EQ(make_lzmw_rans_profile(
                  {2, 2, 16, {}}, limits, stream, workspace),
              LzmwRansProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes = 600;
    EXPECT_EQ(make_lzmw_rans_profile(
                  {1, 1, 8, {}}, limits, stream, workspace),
              LzmwRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);

    limits = {};
    limits.max_frame_size = 2U << 20;
    EXPECT_EQ(make_lzmw_rans_profile(
                  {(1U << 20) + 1U, (1U << 20) + 1U, 65'536, {}},
                  limits, stream, workspace),
              LzmwRansProfileError::limit_exceeded);
}

TEST(LzmwRansProfile, CalculatesCoupledDecoderLayout) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    LzmwRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzmw_rans_decoder_workspace(limits, workspace),
              LzmwRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.block_view_count, 4U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.expansion_entry_count, 11U);
    EXPECT_EQ(workspace.phrase_offset
                  % alignof(marc::dictionary::internal::LzmwPhraseEntry),
              0U);
    EXPECT_EQ(workspace.expansion_offset % alignof(std::uint32_t), 0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max({alignof(marc::entropy::internal::RansBlockView),
                        alignof(marc::dictionary::internal::LzmwPhraseEntry),
                        alignof(std::uint32_t)}));
    EXPECT_EQ(workspace.views_bytes,
              workspace.expansion_offset
                  + workspace.expansion_entry_count * sizeof(std::uint32_t));

    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(calculate_lzmw_rans_decoder_workspace(limits, workspace),
              LzmwRansProfileError::arithmetic_overflow);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzmwRansProfile, PartitionsEncoderAndDecoderOpaqueStorage) {
    StreamHeader stream{};
    LzmwRansEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(make_lzmw_rans_profile(
                  {4, 4, 4, {}}, {}, stream, encoder),
              LzmwRansProfileError::none);
    std::vector<std::byte> encoder_allocation(
        encoder.views_bytes + encoder.views_alignment);
    auto encoder_storage = aligned_storage(
        encoder_allocation, encoder.views_bytes, encoder.views_alignment);
    LzmwRansEncoderViews encoder_views{};
    ASSERT_EQ(partition_lzmw_rans_encoder_views(
                  encoder, encoder_storage, encoder_views),
              LzmwRansWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), encoder.encoder_entry_count);
    EXPECT_EQ(partition_lzmw_rans_encoder_views(
                  encoder,
                  encoder_storage.first(encoder_storage.size() - 1),
                  encoder_views),
              LzmwRansWorkspaceError::too_small);
    auto invalid_encoder = encoder;
    ++invalid_encoder.encoder_entry_count;
    EXPECT_EQ(partition_lzmw_rans_encoder_views(
                  invalid_encoder, encoder_storage, encoder_views),
              LzmwRansWorkspaceError::invalid_requirements);
    if (encoder.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{
            encoder_storage.data() + 1, encoder_storage.size()};
        EXPECT_EQ(partition_lzmw_rans_encoder_views(
                      encoder, misaligned, encoder_views),
                  LzmwRansWorkspaceError::misaligned);
    }

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    LzmwRansDecoderWorkspaceRequirements decoder{};
    ASSERT_EQ(calculate_lzmw_rans_decoder_workspace(limits, decoder),
              LzmwRansProfileError::none);
    std::vector<std::byte> decoder_allocation(
        decoder.views_bytes + decoder.views_alignment);
    auto decoder_storage = aligned_storage(
        decoder_allocation, decoder.views_bytes, decoder.views_alignment);
    LzmwRansDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzmw_rans_decoder_views(
                  decoder, decoder_storage, decoder_views),
              LzmwRansWorkspaceError::none);
    EXPECT_EQ(decoder_views.blocks.size(), decoder.block_view_count);
    EXPECT_EQ(decoder_views.phrases.size(), decoder.phrase_entry_count);
    EXPECT_EQ(decoder_views.expansion.size(), decoder.expansion_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(decoder_views.phrases.data()),
              decoder_storage.data() + decoder.phrase_offset);
    EXPECT_EQ(reinterpret_cast<std::byte*>(decoder_views.expansion.data()),
              decoder_storage.data() + decoder.expansion_offset);

    auto invalid_decoder = decoder;
    ++invalid_decoder.expansion_offset;
    EXPECT_EQ(partition_lzmw_rans_decoder_views(
                  invalid_decoder, decoder_storage, decoder_views),
              LzmwRansWorkspaceError::invalid_requirements);
    EXPECT_EQ(partition_lzmw_rans_decoder_views(
                  decoder,
                  decoder_storage.first(decoder_storage.size() - 1),
                  decoder_views),
              LzmwRansWorkspaceError::too_small);
    if (decoder.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{
            decoder_storage.data() + 1, decoder_storage.size()};
        EXPECT_EQ(partition_lzmw_rans_decoder_views(
                      decoder, misaligned, decoder_views),
                  LzmwRansWorkspaceError::misaligned);
    }
}

TEST(LzmwRansProfile, MapsStableCoreErrors) {
    EXPECT_EQ(lzmw_rans_profile_error_code(LzmwRansProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lzmw_rans_profile_error_code(
                  LzmwRansProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lzmw_rans_profile_error_code(LzmwRansProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lzmw_rans_profile_error_code(
                  LzmwRansProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(lzmw_rans_profile_error_code(
                  LzmwRansProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);
}

TEST(LzmwRansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 16'384;
    limits.max_internal_buffered_bytes = 65'536;
    limits.max_blocks_per_frame = 7;
    limits.max_dictionary_entries = 4096;
    auto parameters = marc::dictionary::internal::LzmwParameters{};
    parameters.maximum_entries = 4096;
    const LzmwRansProfileConfig config{raw.size(), 2, 4, parameters};
    StreamHeader stream{};
    LzmwRansEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(make_lzmw_rans_profile(config, limits, stream, encoder_ws),
              LzmwRansProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    std::vector<std::byte> encode_views_allocation(
        encoder_ws.views_bytes + encoder_ws.views_alignment);
    auto encode_views_storage = aligned_storage(
        encode_views_allocation, encoder_ws.views_bytes,
        encoder_ws.views_alignment);
    LzmwRansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzmw_rans_encoder_views(
                  encoder_ws, encode_views_storage, encode_views),
              LzmwRansWorkspaceError::none);
    LzmwRansFrameStreamingEncoder encoder{
        stream, config.parameters, limits, frame_input, encode_dictionary,
        frame_encoded, encode_views.entries};
    std::vector<std::byte> encoded(65'536);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzmwRansDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(calculate_lzmw_rans_decoder_workspace(limits, decoder_ws),
              LzmwRansProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_decoded(decoder_ws.frame_decoded_bytes);
    std::vector<std::byte> decode_views_allocation(
        decoder_ws.views_bytes + decoder_ws.views_alignment);
    auto decode_views_storage = aligned_storage(
        decode_views_allocation, decoder_ws.views_bytes,
        decoder_ws.views_alignment);
    LzmwRansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzmw_rans_decoder_views(
                  decoder_ws, decode_views_storage, decode_views),
              LzmwRansWorkspaceError::none);
    LzmwRansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.blocks, decode_dictionary,
        frame_decoded, decode_views.phrases, decode_views.expansion};
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

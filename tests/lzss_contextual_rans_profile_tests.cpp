#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::max_align_t>& storage,
    const std::size_t bytes) {
    storage.resize(
        (bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    return std::as_writable_bytes(std::span{storage}).first(bytes);
}

} // namespace

TEST(LzssContextualRansProfile, BuildsCanonicalDefaultWorkspace) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(stream.table_log, 12U);
    EXPECT_EQ(stream.context_count, 31U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 795'556U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.views_bytes,
              65'536U
                  * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzssTypedToken));
}

TEST(LzssContextualRansProfile, UsesShortFrameAndEmptyEncoderExtent) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {17}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9328U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace = {1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {}, {}, stream, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    LzssContextualRansEncoderViews views{};
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  workspace, {}, views),
              LzssContextualRansWorkspaceError::none);
    EXPECT_TRUE(views.tokens.empty());
}

TEST(LzssContextualRansProfile, RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements workspace{};
    LzssContextualRansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualRansProfileError::unsupported);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 211;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 100;
    limits.max_block_size = 100;
    limits.max_internal_buffered_bytes = 10'000;
    EXPECT_EQ(make_lzss_contextual_rans_profile(
                  {100, 100, {}}, limits, stream, workspace),
              LzssContextualRansProfileError::limit_exceeded);
}

TEST(LzssContextualRansProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualRansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 11'116U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_rans_decode_table_entries);
    EXPECT_EQ(workspace.token_count, 1024U);
    const auto table_bytes = workspace.table_count
        * sizeof(marc::entropy::internal::RansDecodeEntry);
    const auto token_alignment =
        alignof(marc::dictionary::internal::LzssTypedToken);
    const auto expected_offset =
        (table_bytes + token_alignment - 1) / token_alignment
        * token_alignment;
    EXPECT_EQ(workspace.token_offset, expected_offset);
    EXPECT_EQ(workspace.views_bytes,
              expected_offset
                  + workspace.token_count
                      * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::entropy::internal::RansDecodeEntry),
                  token_alignment));

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = 770'000;
    EXPECT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, workspace),
              LzssContextualRansProfileError::limit_exceeded);
}

TEST(LzssContextualRansProfile, PartitionsTypedViewsTransactionally) {
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements requirements{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {17}, {}, stream, requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssContextualRansEncoderViews views{};
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  requirements, storage, views),
              LzssContextualRansWorkspaceError::none);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    ++requirements.views_bytes;
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  requirements, storage, views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    --requirements.views_bytes;
    ++requirements.views_alignment;
    EXPECT_EQ(partition_lzss_contextual_rans_encoder_views(
                  requirements, storage, views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 32;
    limits.max_compressed_payload_size = 128;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualRansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::max_align_t> decoder_backing;
    auto decoder_storage = aligned_storage(
        decoder_backing, decoder_requirements.views_bytes);
    LzssContextualRansDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements, decoder_storage, decoder_views),
              LzssContextualRansWorkspaceError::none);
    EXPECT_EQ(decoder_views.tables.size(), decoder_requirements.table_count);
    EXPECT_EQ(decoder_views.tokens.size(), decoder_requirements.token_count);

    auto forged = decoder_requirements;
    ++forged.token_offset;
    EXPECT_EQ(partition_lzss_contextual_rans_decoder_views(
                  forged, decoder_storage, decoder_views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(decoder_views.tables.empty());
    EXPECT_TRUE(decoder_views.tokens.empty());
    forged = decoder_requirements;
    --forged.table_count;
    EXPECT_EQ(partition_lzss_contextual_rans_decoder_views(
                  forged, decoder_storage, decoder_views),
              LzssContextualRansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(decoder_views.tables.empty());
    EXPECT_TRUE(decoder_views.tokens.empty());
    EXPECT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements,
                  decoder_storage.first(decoder_storage.size() - 1),
                  decoder_views),
              LzssContextualRansWorkspaceError::too_small);

    std::vector<std::byte> misaligned(
        decoder_requirements.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements,
                  std::span<std::byte>{misaligned}.subspan(
                      1, decoder_requirements.views_bytes),
                  decoder_views),
              LzssContextualRansWorkspaceError::misaligned);
}

TEST(LzssContextualRansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_contextual_rans_profile(
                  {raw.size(), 2, {}}, limits, stream,
                  encoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_requirements.views_bytes);
    LzssContextualRansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzssContextualRansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_rans_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualRansProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_requirements.views_bytes);
    LzssContextualRansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_rans_decoder_views(
                  decoder_requirements, decode_storage, decode_views),
              LzssContextualRansWorkspaceError::none);
    LzssContextualRansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualRansProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    EXPECT_EQ(lzss_contextual_rans_profile_error_code(
                  LzssContextualRansProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_contextual_rans_profile_error_code(
                  LzssContextualRansProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_contextual_rans_profile_error_code(
                  LzssContextualRansProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_contextual_rans_profile_error_code(
                  LzssContextualRansProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_contextual_rans_profile_error_code(
                  LzssContextualRansProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

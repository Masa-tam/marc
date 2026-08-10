#include "frame/lzss_contextual_tans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_tans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_tans_profile.hpp"

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

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(marc::core::ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualTansProfile, BuildsCanonicalDefaultWorkspace) {
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(stream.table_log, 12U);
    EXPECT_EQ(stream.context_count, 31U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 598'919U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_tans_encode_table_entries);
    EXPECT_EQ(workspace.table_offset,
              workspace.token_count
                  * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.views_bytes,
              workspace.table_offset
                  + workspace.table_count * sizeof(std::uint16_t));
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::dictionary::internal::LzssTypedToken),
                  alignof(std::uint16_t)));
}

TEST(LzssContextualTansProfile, UsesShortFrameAndEmptyEncoderExtent) {
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  {17}, {}, stream, workspace),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9248U);
    EXPECT_EQ(workspace.token_count, 17U);

    workspace = {1, 1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  {}, {}, stream, workspace),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.token_count, 0U);
    EXPECT_EQ(workspace.table_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    LzssContextualTansEncoderViews views{};
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  workspace, {}, views),
              LzssContextualTansWorkspaceError::none);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.tables.empty());
}

TEST(LzssContextualTansProfile, RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements workspace{};
    LzssContextualTansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualTansProfileError::unsupported);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 154;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualTansProfileError::limit_exceeded);

    limits = {};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_tans_encode_table_entries - 1;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  {17}, limits, stream, workspace),
              LzssContextualTansProfileError::limit_exceeded);
}

TEST(LzssContextualTansProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualTansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, workspace),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 11'093U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.table_count,
              marc::entropy::internal::
                  contextual_tans_decode_table_entries);
    EXPECT_EQ(workspace.token_count, 1024U);
    const auto table_bytes = workspace.table_count
        * sizeof(marc::entropy::internal::TansDecodeEntry);
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

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, workspace),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualTansProfile, PartitionsTypedViewsTransactionally) {
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements requirements{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  {17}, {}, stream, requirements),
              LzssContextualTansProfileError::none);
    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssContextualTansEncoderViews views{};
    ASSERT_EQ(partition_lzss_contextual_tans_encoder_views(
                  requirements, storage, views),
              LzssContextualTansWorkspaceError::none);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    EXPECT_EQ(views.tables.size(), requirements.table_count);
    auto forged_encoder = requirements;
    ++forged_encoder.table_offset;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.tables.empty());
    forged_encoder = requirements;
    --forged_encoder.table_count;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    forged_encoder = requirements;
    ++forged_encoder.views_bytes;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    forged_encoder = requirements;
    ++forged_encoder.views_alignment;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 32;
    limits.max_compressed_payload_size = 128;
    limits.max_internal_buffered_bytes = 1U << 20;
    LzssContextualTansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualTansProfileError::none);
    std::vector<std::max_align_t> decoder_backing;
    auto decoder_storage = aligned_storage(
        decoder_backing, decoder_requirements.views_bytes);
    LzssContextualTansDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzss_contextual_tans_decoder_views(
                  decoder_requirements, decoder_storage, decoder_views),
              LzssContextualTansWorkspaceError::none);
    EXPECT_EQ(decoder_views.tables.size(), decoder_requirements.table_count);
    EXPECT_EQ(decoder_views.tokens.size(), decoder_requirements.token_count);

    auto forged_decoder = decoder_requirements;
    ++forged_decoder.token_offset;
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  forged_decoder, decoder_storage, decoder_views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(decoder_views.tables.empty());
    EXPECT_TRUE(decoder_views.tokens.empty());
    forged_decoder = decoder_requirements;
    --forged_decoder.table_count;
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  forged_decoder, decoder_storage, decoder_views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    forged_decoder = decoder_requirements;
    ++forged_decoder.views_bytes;
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  forged_decoder, decoder_storage, decoder_views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    forged_decoder = decoder_requirements;
    ++forged_decoder.views_alignment;
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  forged_decoder, decoder_storage, decoder_views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  decoder_requirements,
                  decoder_storage.first(decoder_storage.size() - 1),
                  decoder_views),
              LzssContextualTansWorkspaceError::too_small);

    std::vector<std::byte> misaligned(
        decoder_requirements.views_bytes + 1);
    EXPECT_EQ(partition_lzss_contextual_tans_decoder_views(
                  decoder_requirements,
                  std::span<std::byte>{misaligned}.subspan(
                      1, decoder_requirements.views_bytes),
                  decoder_views),
              LzssContextualTansWorkspaceError::misaligned);
}

TEST(LzssContextualTansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 2U << 20;
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  {raw.size(), 2, {}}, limits, stream,
                  encoder_requirements),
              LzssContextualTansProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_requirements.views_bytes);
    LzssContextualTansEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_contextual_tans_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssContextualTansWorkspaceError::none);
    LzssContextualTansFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.tables, frame_encoded};
    std::vector<std::byte> encoded(40'000);
    const auto encoded_result = encoder.process(
        raw, encoded, end_flag());
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzssContextualTansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder_requirements),
              LzssContextualTansProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_requirements.views_bytes);
    LzssContextualTansDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_contextual_tans_decoder_views(
                  decoder_requirements, decode_storage, decode_views),
              LzssContextualTansWorkspaceError::none);
    LzssContextualTansFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tables, decode_views.tokens,
        frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded, end_flag());
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    EXPECT_EQ(lzss_contextual_tans_profile_error_code(
                  LzssContextualTansProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_contextual_tans_profile_error_code(
                  LzssContextualTansProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_contextual_tans_profile_error_code(
                  LzssContextualTansProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_contextual_tans_profile_error_code(
                  LzssContextualTansProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_contextual_tans_profile_error_code(
                  LzssContextualTansProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

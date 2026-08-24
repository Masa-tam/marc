#include "frame/lzss_contextual_tans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_tans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_tans_profile.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

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
              workspace.match_finder_offset + workspace.match_finder_bytes);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error, marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  alignof(marc::dictionary::internal::LzssTypedToken),
                  std::max(alignof(std::uint16_t), finder.workspace_alignment)));
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

    workspace.frame_input_bytes = 1;
    workspace.frame_encoded_bytes = 1;
    workspace.token_count = 1;
    workspace.table_count = 1;
    workspace.table_offset = 1;
    workspace.match_finder_offset = 1;
    workspace.match_finder_bytes = 1;
    workspace.views_bytes = 1;
    workspace.views_alignment = 8;
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
    EXPECT_TRUE(views.match_finder.empty());

    LzssContextualTansProfileConfig extended{};
    extended.original_size = 17;
    extended.frame_size = 17;
    extended.dictionary.window_size = UINT32_C(1) << 20;
    extended.variant =
        LzssContextualTansProfileVariant::field_context_1m;
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  extended, {}, stream, workspace),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 3U);
    EXPECT_EQ(stream.context_variant, 2U);
    EXPECT_EQ(stream.frequency_entry_count, 4550U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9312U);
}

TEST(LzssContextualTansProfile, RejectsUnsupportedAndBoundedConfigurations) {
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements workspace{};
    LzssContextualTansProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualTansProfileError::unsupported);
    unsupported = {};
    unsupported.variant =
        static_cast<LzssContextualTansProfileVariant>(255);
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  unsupported, {}, stream, workspace),
              LzssContextualTansProfileError::unsupported);
    EXPECT_EQ(workspace.views_bytes, 0U);
    LzssContextualTansDecoderWorkspaceRequirements decoder_workspace{};
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  {}, decoder_workspace,
                  static_cast<LzssContextualTansProfileVariant>(255)),
              LzssContextualTansProfileError::unsupported);
    EXPECT_EQ(decoder_workspace.views_bytes, 0U);

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

    LzssContextualTansDecoderWorkspaceRequirements extended{};
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, extended,
                  LzssContextualTansProfileVariant::field_context_1m),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(extended.frame_encoded_bytes, 11'157U);
    EXPECT_EQ(extended.table_count, workspace.table_count);
    EXPECT_EQ(extended.token_count, workspace.token_count);

    limits.max_entropy_table_entries = workspace.table_count - 1;
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, workspace),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(LzssContextualTansProfile,
     FourMiBProfileFitsDefaultAggregateWithExactBoundaries) {
    constexpr std::uint64_t frame_size = UINT64_C(1) << 22;
    constexpr std::uint64_t decision_limit = 7 * frame_size;
    LzssContextualTansProfileConfig config{};
    config.original_size = frame_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.dictionary.window_size = static_cast<std::uint32_t>(frame_size);
    config.variant = LzssContextualTansProfileVariant::field_context_4m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = frame_size;
    limits.max_block_size = decision_limit;
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 4U);
    EXPECT_EQ(stream.context_variant, 3U);
    EXPECT_EQ(stream.frequency_entry_count, 4566U);
    EXPECT_EQ(encoder.frame_input_bytes, 4'194'304U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 44'049'383U);
    EXPECT_EQ(encoder.token_count, 4'194'304U);
    EXPECT_EQ(encoder.table_count, 131'072U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.table_offset, 50'331'648U);
        EXPECT_EQ(encoder.match_finder_offset, 50'593'792U);
        EXPECT_EQ(encoder.match_finder_bytes, 17'301'504U);
        EXPECT_EQ(encoder.views_bytes, 67'895'296U);
        EXPECT_EQ(encoder_aggregate, 116'138'983U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::none);

    LzssContextualTansDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_4m),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 44'049'383U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 4'194'304U);
    EXPECT_EQ(decoder.table_count, 131'072U);
    EXPECT_EQ(decoder.token_count, 4'194'304U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (sizeof(marc::entropy::internal::TansDecodeEntry) == 4
                  && sizeof(marc::dictionary::internal::LzssTypedToken)
                         == 12) {
        EXPECT_EQ(decoder.token_offset, 524'288U);
        EXPECT_EQ(decoder.views_bytes, 50'855'936U);
        EXPECT_EQ(decoder_aggregate, 99'099'623U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_4m),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_4m),
              LzssContextualTansProfileError::none);
}

TEST(LzssContextualTansProfile,
     SixteenMiBProfileRequiresExplicitAggregateWithExactBoundaries) {
    constexpr std::uint64_t frame_size = UINT64_C(1) << 24;
    constexpr std::uint64_t decision_limit = 7 * frame_size;
    LzssContextualTansProfileConfig config{};
    config.original_size = frame_size;
    config.frame_size = static_cast<std::uint32_t>(frame_size);
    config.dictionary.window_size = static_cast<std::uint32_t>(frame_size);
    config.variant = LzssContextualTansProfileVariant::field_context_16m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = frame_size;
    limits.max_block_size = decision_limit;
    limits.max_compressed_payload_size = 176'160'770U;
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements encoder{};
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 5U);
    EXPECT_EQ(stream.context_variant, 4U);
    EXPECT_EQ(stream.frequency_entry_count, 4582U);
    EXPECT_EQ(encoder.frame_input_bytes, 16'777'216U);
    EXPECT_EQ(encoder.frame_encoded_bytes, 176'169'991U);
    EXPECT_EQ(encoder.token_count, 16'777'216U);
    EXPECT_EQ(encoder.table_count, 131'072U);
    const auto encoder_aggregate = encoder.frame_input_bytes
        + encoder.views_bytes + encoder.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(encoder.table_offset, 201'326'592U);
        EXPECT_EQ(encoder.match_finder_offset, 201'588'736U);
        EXPECT_EQ(encoder.match_finder_bytes, 67'633'152U);
        EXPECT_EQ(encoder.views_bytes, 269'221'888U);
        EXPECT_EQ(encoder_aggregate, 462'169'095U);
    }
    limits.max_internal_buffered_bytes = encoder_aggregate - 1;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(encoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = encoder_aggregate;
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::none);

    LzssContextualTansDecoderWorkspaceRequirements decoder{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_16m),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = UINT64_C(512) << 20;
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_16m),
              LzssContextualTansProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 176'169'991U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 16'777'216U);
    EXPECT_EQ(decoder.table_count, 131'072U);
    EXPECT_EQ(decoder.token_count, 16'777'216U);
    const auto decoder_aggregate = decoder.frame_encoded_bytes
        + decoder.frame_decoded_bytes + decoder.views_bytes;
    if constexpr (sizeof(marc::entropy::internal::TansDecodeEntry) == 4
                  && sizeof(marc::dictionary::internal::LzssTypedToken)
                         == 12) {
        EXPECT_EQ(decoder.token_offset, 524'288U);
        EXPECT_EQ(decoder.views_bytes, 201'850'880U);
        EXPECT_EQ(decoder_aggregate, 394'798'087U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_16m),
              LzssContextualTansProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder,
                  LzssContextualTansProfileVariant::field_context_16m),
              LzssContextualTansProfileError::none);

    limits.max_block_size = decision_limit - 1;
    EXPECT_EQ(make_lzss_contextual_tans_profile(
                  config, limits, stream, encoder),
              LzssContextualTansProfileError::limit_exceeded);
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
    EXPECT_EQ(views.match_finder.size(), requirements.match_finder_bytes);
    EXPECT_EQ(views.match_finder.data(),
              storage.data() + requirements.match_finder_offset);
    auto forged_encoder = requirements;
    ++forged_encoder.table_offset;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.tables.empty());
    EXPECT_TRUE(views.match_finder.empty());
    forged_encoder = requirements;
    --forged_encoder.table_count;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    forged_encoder = requirements;
    ++forged_encoder.match_finder_offset;
    EXPECT_EQ(partition_lzss_contextual_tans_encoder_views(
                  forged_encoder, storage, views),
              LzssContextualTansWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.tables.empty());
    EXPECT_TRUE(views.match_finder.empty());
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
        encode_views.tables, encode_views.match_finder, frame_encoded};
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

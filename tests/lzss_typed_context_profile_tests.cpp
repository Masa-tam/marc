#include "frame/lzss_typed_context_frame_streaming_decoder.hpp"
#include "frame/lzss_typed_context_frame_streaming_encoder.hpp"
#include "frame/lzss_typed_context_profile.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

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

TEST(LzssTypedContextProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_typed_context_profile(
                  {2'500'000}, {}, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.original_size, 2'500'000U);
    EXPECT_EQ(stream.range_model_total, typed_context_model_total);
    EXPECT_EQ(stream.context_count, typed_context_count);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 786'517U);
    EXPECT_EQ(workspace.token_count, 65'536U);
    EXPECT_EQ(workspace.operation_count, 131'072U);
    EXPECT_EQ(workspace.operation_offset,
              65'536U * sizeof(marc::dictionary::internal::LzssTypedToken));
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(65'536, {}, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_offset,
              workspace.operation_offset
                  + 131'072U
                      * sizeof(marc::context::internal::ModeledOperation));
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + finder.workspace_size);
    EXPECT_EQ(workspace.views_alignment,
              std::max(
                  std::max(
                      alignof(marc::dictionary::internal::LzssTypedToken),
                      alignof(marc::context::internal::ModeledOperation)),
                  finder.workspace_alignment));
}

TEST(LzssTypedContextProfile, BuildsExtendedOneMiBProfileAndWorkspace) {
    LzssTypedContextProfileConfig config{};
    config.original_size = 1048576;
    config.frame_size = 1048576;
    config.dictionary.window_size = 1048576;
    config.variant = LzssTypedContextProfileVariant::field_context_1m;
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_typed_context_profile(
                  config, {}, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 3U);
    EXPECT_EQ(stream.context_algorithm, 1U);
    EXPECT_EQ(stream.context_variant, 2U);
    EXPECT_EQ(stream.frame_size, 1048576U);
    EXPECT_EQ(stream.dictionary.window_size, 1048576U);
    EXPECT_EQ(workspace.frame_input_bytes, 1048576U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 12582997U);
    EXPECT_EQ(workspace.token_count, 1048576U);
    EXPECT_EQ(workspace.operation_count, 2097152U);
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            1048576, config.dictionary, {});
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + finder.workspace_size);
    EXPECT_LE(1048576U + workspace.views_bytes
                  + workspace.frame_encoded_bytes,
              marc::core::DecoderLimits{}.max_internal_buffered_bytes);
}

TEST(LzssTypedContextProfile,
     FourMiBProfileRequiresExplicitAggregateLimit) {
    LzssTypedContextProfileConfig config{};
    config.original_size = 4'194'304;
    config.frame_size = 4'194'304;
    config.dictionary.window_size = 4'194'304;
    config.variant = LzssTypedContextProfileVariant::field_context_4m;
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 4'194'304;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits.max_internal_buffered_bytes = UINT64_C(256) << 20;
    ASSERT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::none);
    const auto required_aggregate = workspace.frame_input_bytes
        + workspace.views_bytes + workspace.frame_encoded_bytes;
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(required_aggregate, 264'765'525U);
    }

    limits.max_internal_buffered_bytes = required_aggregate - 1;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits.max_internal_buffered_bytes = required_aggregate;
    ASSERT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 4U);
    EXPECT_EQ(stream.context_variant, 3U);
    EXPECT_EQ(workspace.frame_input_bytes, 4'194'304U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 58'720'341U);
    EXPECT_EQ(workspace.token_count, 4'194'304U);
    EXPECT_EQ(workspace.operation_count, 8'388'608U);
    EXPECT_EQ(workspace.operation_offset,
              4'194'304U
                  * sizeof(marc::dictionary::internal::LzssTypedToken));
    const auto finder = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            4'194'304, config.dictionary, limits);
    ASSERT_EQ(finder.error,
              marc::dictionary::internal::LzssHashChainError::none);
    EXPECT_EQ(workspace.match_finder_offset,
              workspace.operation_offset
                  + 8'388'608U
                      * sizeof(marc::context::internal::ModeledOperation));
    EXPECT_EQ(workspace.match_finder_bytes, finder.workspace_size);
    EXPECT_EQ(workspace.views_bytes,
              workspace.match_finder_offset + finder.workspace_size);
    EXPECT_EQ(workspace.frame_input_bytes + workspace.views_bytes
                  + workspace.frame_encoded_bytes,
              required_aggregate);

    limits.max_compressed_payload_size = UINT64_C(64) << 20;
    LzssTypedContextDecoderWorkspaceRequirements decoder_workspace{};
    limits.max_internal_buffered_bytes = UINT64_C(128) << 20;
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder_workspace,
                  LzssTypedContextProfileVariant::field_context_4m),
              LzssTypedContextProfileError::none);
    const auto decoder_aggregate = decoder_workspace.frame_encoded_bytes
        + decoder_workspace.frame_decoded_bytes
        + decoder_workspace.views_bytes;
    if constexpr (sizeof(marc::dictionary::internal::LzssTypedToken) == 12) {
        EXPECT_EQ(decoder_aggregate, 121'634'896U);
    }
    limits.max_internal_buffered_bytes = decoder_aggregate - 1;
    EXPECT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder_workspace,
                  LzssTypedContextProfileVariant::field_context_4m),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(decoder_workspace.views_bytes, 0U);
    limits.max_internal_buffered_bytes = decoder_aggregate;
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder_workspace,
                  LzssTypedContextProfileVariant::field_context_4m),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(decoder_workspace.frame_encoded_bytes, 67'108'944U);
    EXPECT_EQ(decoder_workspace.frame_decoded_bytes, 4'194'304U);
    EXPECT_EQ(decoder_workspace.views_bytes, 50'331'648U);
}

TEST(LzssTypedContextProfile,
     SixteenMiBProfileUsesExactEncoderAndDecoderAggregates) {
    LzssTypedContextProfileConfig config{};
    config.original_size = 16'777'216;
    config.frame_size = 16'777'216;
    config.dictionary.window_size = 16'777'216;
    config.variant = LzssTypedContextProfileVariant::field_context_16m;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 16'777'216;
    limits.max_block_size = 16'777'216;
    limits.max_compressed_payload_size = 234'881'029;
    limits.max_lz_distance = 16'777'216;
    limits.max_entropy_table_entries = 4'582;
    limits.max_internal_buffered_bytes = 1'057'488'981;
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};

    ASSERT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(stream.dictionary_variant, 5U);
    EXPECT_EQ(stream.context_variant, 4U);
    EXPECT_EQ(workspace.frame_input_bytes, 16'777'216U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 234'881'109U);
    EXPECT_EQ(workspace.token_count, 16'777'216U);
    EXPECT_EQ(workspace.operation_count, 33'554'432U);
    EXPECT_EQ(workspace.match_finder_bytes, 67'633'152U);
    EXPECT_EQ(workspace.views_bytes, 805'830'656U);
    EXPECT_EQ(workspace.frame_input_bytes + workspace.views_bytes
                  + workspace.frame_encoded_bytes,
              1'057'488'981U);

    limits.max_internal_buffered_bytes = 1'057'488'980;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  config, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(stream.original_size, 16'777'216U);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits.max_internal_buffered_bytes = 452'984'917;
    LzssTypedContextDecoderWorkspaceRequirements decoder{};
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder,
                  LzssTypedContextProfileVariant::field_context_16m),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(decoder.frame_encoded_bytes, 234'881'109U);
    EXPECT_EQ(decoder.frame_decoded_bytes, 16'777'216U);
    EXPECT_EQ(decoder.token_count, 16'777'216U);
    EXPECT_EQ(decoder.views_bytes, 201'326'592U);
    EXPECT_EQ(decoder.frame_encoded_bytes + decoder.frame_decoded_bytes
                  + decoder.views_bytes,
              452'984'917U);

    limits.max_internal_buffered_bytes = 452'984'916;
    EXPECT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder,
                  LzssTypedContextProfileVariant::field_context_16m),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(decoder.views_bytes, 0U);
}

TEST(LzssTypedContextProfile, UsesActualShortFrameAndEmptyExtent) {
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzss_typed_context_profile(
                  {17}, {}, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 289U);
    EXPECT_EQ(workspace.token_count, 17U);
    EXPECT_EQ(workspace.operation_count, 34U);

    workspace.frame_input_bytes = 1;
    workspace.frame_encoded_bytes = 1;
    workspace.token_count = 1;
    workspace.operation_count = 1;
    workspace.operation_offset = 1;
    workspace.match_finder_offset = 1;
    workspace.match_finder_bytes = 1;
    workspace.views_bytes = 1;
    workspace.views_alignment = 8;
    ASSERT_EQ(make_lzss_typed_context_profile(
                  {}, {}, stream, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    LzssTypedContextEncoderViews empty_views{};
    EXPECT_EQ(partition_lzss_typed_context_encoder_views(
                  workspace, {}, empty_views),
              LzssTypedContextWorkspaceError::none);
    EXPECT_TRUE(empty_views.tokens.empty());
    EXPECT_TRUE(empty_views.operations.empty());
    EXPECT_TRUE(empty_views.match_finder.empty());
}

TEST(LzssTypedContextProfile, RejectsUnsupportedAndBoundedConfigurations) {
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements workspace{};
    LzssTypedContextProfileConfig unsupported{};
    unsupported.dictionary.max_match_length = 259;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  unsupported, {}, stream, workspace),
              LzssTypedContextProfileError::unsupported);

    unsupported = {};
    unsupported.variant =
        static_cast<LzssTypedContextProfileVariant>(255);
    EXPECT_EQ(make_lzss_typed_context_profile(
                  unsupported, {}, stream, workspace),
              LzssTypedContextProfileError::unsupported);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 208;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  {17}, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  {17}, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 100;
    limits.max_internal_buffered_bytes = 5'784;
    EXPECT_EQ(make_lzss_typed_context_profile(
                  {100}, limits, stream, workspace),
              LzssTypedContextProfileError::limit_exceeded);
}

TEST(LzssTypedContextProfile, CalculatesDecoderWorkspaceFromLimits) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 1024;
    limits.max_compressed_payload_size = 2000;
    limits.max_internal_buffered_bytes = 15'392;
    LzssTypedContextDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, workspace),
              LzssTypedContextProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2080U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 1024U);
    EXPECT_EQ(workspace.token_count, 1024U);
    EXPECT_EQ(workspace.views_bytes,
              1024U * sizeof(marc::dictionary::internal::LzssTypedToken));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzssTypedToken));

    limits.max_entropy_table_entries = typed_context_table_entries - 1;
    EXPECT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, workspace),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(workspace.views_bytes, 0U);

    limits = {};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v2 - 1;
    EXPECT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, workspace,
                  LzssTypedContextProfileVariant::field_context_1m),
              LzssTypedContextProfileError::limit_exceeded);
    EXPECT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, workspace,
                  LzssTypedContextProfileVariant::field_context_64k),
              LzssTypedContextProfileError::none);
}

TEST(LzssTypedContextProfile, PartitionsTypedViewsTransactionally) {
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements requirements{};
    ASSERT_EQ(make_lzss_typed_context_profile(
                  {17}, {}, stream, requirements),
              LzssTypedContextProfileError::none);
    std::vector<std::max_align_t> backing;
    auto storage = aligned_storage(backing, requirements.views_bytes);
    LzssTypedContextEncoderViews views{};
    ASSERT_EQ(partition_lzss_typed_context_encoder_views(
                  requirements, storage, views),
              LzssTypedContextWorkspaceError::none);
    EXPECT_EQ(views.tokens.size(), requirements.token_count);
    EXPECT_EQ(views.operations.size(), requirements.operation_count);
    EXPECT_EQ(views.match_finder.size(), requirements.match_finder_bytes);
    EXPECT_EQ(views.match_finder.data(),
              storage.data() + requirements.match_finder_offset);

    auto forged = requirements;
    ++forged.operation_offset;
    EXPECT_EQ(partition_lzss_typed_context_encoder_views(
                  forged, storage, views),
              LzssTypedContextWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.tokens.empty());
    EXPECT_TRUE(views.operations.empty());
    EXPECT_TRUE(views.match_finder.empty());
    forged = requirements;
    ++forged.match_finder_offset;
    EXPECT_EQ(partition_lzss_typed_context_encoder_views(
                  forged, storage, views),
              LzssTypedContextWorkspaceError::invalid_requirements);
    EXPECT_TRUE(views.match_finder.empty());
    EXPECT_EQ(partition_lzss_typed_context_encoder_views(
                  requirements, storage.first(storage.size() - 1), views),
              LzssTypedContextWorkspaceError::too_small);

    alignas(64) std::array<std::byte, 1024> misaligned{};
    auto small_requirements = requirements;
    small_requirements.token_count = 1;
    small_requirements.operation_count = 2;
    small_requirements.operation_offset =
        sizeof(marc::dictionary::internal::LzssTypedToken);
    const auto operation_end = small_requirements.operation_offset
        + 2 * sizeof(marc::context::internal::ModeledOperation);
    const auto finder_alignment = marc::dictionary::internal::
        LzssHashChainWorkspaceRequirements{}.workspace_alignment;
    small_requirements.match_finder_offset =
        (operation_end + finder_alignment - 1) / finder_alignment
        * finder_alignment;
    small_requirements.match_finder_bytes = 0;
    small_requirements.views_bytes = small_requirements.match_finder_offset;
    EXPECT_EQ(partition_lzss_typed_context_encoder_views(
                  small_requirements,
                  std::span<std::byte>{misaligned}.subspan(
                      1, small_requirements.views_bytes),
                  views),
              LzssTypedContextWorkspaceError::misaligned);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 32;
    limits.max_block_size = 32;
    limits.max_compressed_payload_size = 64;
    limits.max_internal_buffered_bytes = 560;
    LzssTypedContextDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder_requirements),
              LzssTypedContextProfileError::none);
    std::vector<std::max_align_t> decoder_backing;
    auto decoder_storage = aligned_storage(
        decoder_backing, decoder_requirements.views_bytes);
    LzssTypedContextDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzss_typed_context_decoder_views(
                  decoder_requirements, decoder_storage, decoder_views),
              LzssTypedContextWorkspaceError::none);
    EXPECT_EQ(decoder_views.tokens.size(), decoder_requirements.token_count);
    ++decoder_requirements.views_bytes;
    EXPECT_EQ(partition_lzss_typed_context_decoder_views(
                  decoder_requirements, decoder_storage, decoder_views),
              LzssTypedContextWorkspaceError::invalid_requirements);
    EXPECT_TRUE(decoder_views.tokens.empty());
}

TEST(LzssTypedContextProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 8192;
    limits.max_internal_buffered_bytes = 65'536;
    TypedContextStreamHeader stream{};
    LzssTypedContextEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_typed_context_profile(
                  {raw.size(), 2, {}}, limits, stream,
                  encoder_requirements),
              LzssTypedContextProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    std::vector<std::max_align_t> encode_backing;
    auto encode_storage = aligned_storage(
        encode_backing, encoder_requirements.views_bytes);
    LzssTypedContextEncoderViews encode_views{};
    ASSERT_EQ(partition_lzss_typed_context_encoder_views(
                  encoder_requirements, encode_storage, encode_views),
              LzssTypedContextWorkspaceError::none);
    LzssTypedContextFrameStreamingEncoder encoder{
        stream, limits, frame_input, encode_views.tokens,
        encode_views.operations, encode_views.match_finder, frame_encoded};
    std::vector<std::byte> encoded(4096);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(encoded_result.output_produced);

    LzssTypedContextDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_typed_context_decoder_workspace(
                  limits, decoder_requirements),
              LzssTypedContextProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> frame_decoded(
        decoder_requirements.frame_decoded_bytes);
    std::vector<std::max_align_t> decode_backing;
    auto decode_storage = aligned_storage(
        decode_backing, decoder_requirements.views_bytes);
    LzssTypedContextDecoderViews decode_views{};
    ASSERT_EQ(partition_lzss_typed_context_decoder_views(
                  decoder_requirements, decode_storage, decode_views),
              LzssTypedContextWorkspaceError::none);
    LzssTypedContextFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_views.tokens, frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssTypedContextProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    EXPECT_EQ(lzss_typed_context_profile_error_code(
                  LzssTypedContextProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_typed_context_profile_error_code(
                  LzssTypedContextProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_typed_context_profile_error_code(
                  LzssTypedContextProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_typed_context_profile_error_code(
                  LzssTypedContextProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_typed_context_profile_error_code(
                  LzssTypedContextProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

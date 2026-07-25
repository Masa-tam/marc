#include "frame/lzw_dynamic_range_profile.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

TEST(LzwDynamicRangeProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    StreamHeader stream{};
    LzwDynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzw_dynamic_range_profile(
                  {17, 10, {}}, {}, stream, workspace),
              LzwDynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzw);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 20U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 45U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzwEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzwEncoderEntry));
}

TEST(LzwDynamicRangeProfile, UsesShortFrameAndCanonicalEmptyLayout) {
    StreamHeader stream{};
    LzwDynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzw_dynamic_range_profile(
                  {7, 16, {}}, {}, stream, workspace),
              LzwDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 14U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 33U);
    EXPECT_EQ(workspace.encoder_entry_count, 6U);

    workspace = {1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lzw_dynamic_range_profile(
                  {}, {}, stream, workspace),
              LzwDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzwDynamicRangeProfile, EnforcesPackedPayloadAndAggregateLimits) {
    StreamHeader stream{};
    LzwDynamicRangeEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 1;
    EXPECT_EQ(make_lzw_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwDynamicRangeProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 8;
    EXPECT_EQ(make_lzw_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwDynamicRangeProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        1 + 2 + (56 + 16 + 9) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(make_lzw_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwDynamicRangeProfileError::limit_exceeded);
}

TEST(LzwDynamicRangeProfile, CalculatesDecoderLayoutFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_block_size = 128;
    limits.max_dictionary_entries = 300;
    LzwDynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzw_dynamic_range_decoder_workspace(
                  limits, workspace),
              LzwDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.phrase_entry_count, 112U);
    EXPECT_EQ(workspace.views_bytes,
              112U * sizeof(marc::dictionary::internal::LzwPhraseEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzwPhraseEntry));
}

TEST(LzwDynamicRangeProfile, PartitionsAndRejectsInvalidTypedStorage) {
    LzwDynamicRangeEncoderWorkspaceRequirements encoder{};
    encoder.encoder_entry_count = 2;
    encoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::LzwEncoderEntry);
    encoder.views_alignment =
        alignof(marc::dictionary::internal::LzwEncoderEntry);
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> entries{};
    LzwDynamicRangeEncoderViews encoder_views{};
    ASSERT_EQ(partition_lzw_dynamic_range_encoder_views(
                  encoder, std::as_writable_bytes(std::span{entries}),
                  encoder_views),
              LzwDynamicRangeWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), 2U);

    LzwDynamicRangeDecoderWorkspaceRequirements decoder{};
    decoder.phrase_entry_count = 2;
    decoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::LzwPhraseEntry);
    decoder.views_alignment =
        alignof(marc::dictionary::internal::LzwPhraseEntry);
    std::array<marc::dictionary::internal::LzwPhraseEntry, 2> phrases{};
    LzwDynamicRangeDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzw_dynamic_range_decoder_views(
                  decoder, std::as_writable_bytes(std::span{phrases}),
                  decoder_views),
              LzwDynamicRangeWorkspaceError::none);
    EXPECT_EQ(decoder_views.phrases.size(), 2U);

    auto changed = decoder;
    ++changed.phrase_entry_count;
    EXPECT_EQ(partition_lzw_dynamic_range_decoder_views(
                  changed, std::as_writable_bytes(std::span{phrases}),
                  decoder_views),
              LzwDynamicRangeWorkspaceError::invalid_requirements);
    EXPECT_EQ(partition_lzw_dynamic_range_decoder_views(
                  decoder,
                  std::as_writable_bytes(std::span{phrases}).first(
                      decoder.views_bytes - 1),
                  decoder_views),
              LzwDynamicRangeWorkspaceError::too_small);
    std::vector<std::byte> misaligned(decoder.views_bytes + 1);
    EXPECT_EQ(partition_lzw_dynamic_range_decoder_views(
                  decoder, std::span<std::byte>{misaligned}.subspan(1),
                  decoder_views),
              LzwDynamicRangeWorkspaceError::misaligned);
}

TEST(LzwDynamicRangeProfile, MapsStableErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(lzw_dynamic_range_profile_error_code(
                  LzwDynamicRangeProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lzw_dynamic_range_profile_error_code(
                  LzwDynamicRangeProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lzw_dynamic_range_profile_error_code(
                  LzwDynamicRangeProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lzw_dynamic_range_profile_error_code(
                  LzwDynamicRangeProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(lzw_dynamic_range_profile_error_code(
                  LzwDynamicRangeProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 253;
    LzwDynamicRangeDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzw_dynamic_range_decoder_workspace(
                  limits, workspace),
              LzwDynamicRangeProfileError::limit_exceeded);
}

} // namespace

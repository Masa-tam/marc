#include "frame/lz78_dynamic_range_profile.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

TEST(Lz78DynamicRangeProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    StreamHeader stream{};
    Lz78DynamicRangeEncoderWorkspaceRequirements workspace{};
    const Lz78DynamicRangeProfileConfig config{2'500'000};
    ASSERT_EQ(make_lz78_dynamic_range_profile(
                  config, {}, stream, workspace),
              Lz78DynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lz78);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 524'288U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 1'048'653U);
    EXPECT_EQ(workspace.encoder_entry_count, 65'536U);
    EXPECT_EQ(workspace.views_bytes,
              workspace.encoder_entry_count
                  * sizeof(marc::dictionary::internal::Lz78EncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::Lz78EncoderEntry));
}

TEST(Lz78DynamicRangeProfile, UsesShortFrameAndCanonicalEmptyLayout) {
    StreamHeader stream{};
    Lz78DynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lz78_dynamic_range_profile(
                  {17}, {}, stream, workspace),
              Lz78DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 136U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 349U);
    EXPECT_EQ(workspace.encoder_entry_count, 17U);

    workspace = {1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lz78_dynamic_range_profile(
                  {}, {}, stream, workspace),
              Lz78DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(Lz78DynamicRangeProfile, RejectsLimitsAndInvalidParameters) {
    StreamHeader stream{};
    Lz78DynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4U << 20;
    EXPECT_EQ(make_lz78_dynamic_range_profile(
                  {(1U << 21) + 1U, (1U << 21) + 1U, {}},
                  limits, stream, workspace),
              Lz78DynamicRangeProfileError::limit_exceeded);

    Lz78DynamicRangeProfileConfig invalid{};
    invalid.parameters.maximum_entries = 0;
    EXPECT_EQ(make_lz78_dynamic_range_profile(
                  invalid, {}, stream, workspace),
              Lz78DynamicRangeProfileError::invalid_configuration);

    limits = {};
    limits.max_compressed_payload_size = 276;
    EXPECT_EQ(make_lz78_dynamic_range_profile(
                  {17}, limits, stream, workspace),
              Lz78DynamicRangeProfileError::limit_exceeded);
}

TEST(Lz78DynamicRangeProfile, EnforcesAggregateWorkspaceBound) {
    StreamHeader stream{};
    Lz78DynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 32;
    limits.max_block_size = 32;
    limits.max_internal_buffered_bytes =
        17 + 136 + 349
        + 17 * sizeof(marc::dictionary::internal::Lz78EncoderEntry) - 1;
    EXPECT_EQ(make_lz78_dynamic_range_profile(
                  {17}, limits, stream, workspace),
              Lz78DynamicRangeProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
}

TEST(Lz78DynamicRangeProfile, DerivesBoundedDecoderWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4'096;
    limits.max_block_size = 4'096;
    limits.max_dictionary_serialized_size = 6'000;
    limits.max_internal_buffered_bytes = 8'192;
    Lz78DynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lz78_dynamic_range_decoder_workspace(
                  limits, workspace),
              Lz78DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8'192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6'000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4'096U);
    EXPECT_EQ(workspace.phrase_entry_count, 750U);
    EXPECT_EQ(workspace.views_bytes,
              750U * sizeof(marc::dictionary::internal::Lz78PhraseEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::Lz78PhraseEntry));
}

TEST(Lz78DynamicRangeProfile, PartitionsAndRejectsInvalidTypedStorage) {
    Lz78DynamicRangeEncoderWorkspaceRequirements encoder{};
    encoder.encoder_entry_count = 2;
    encoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::Lz78EncoderEntry);
    encoder.views_alignment =
        alignof(marc::dictionary::internal::Lz78EncoderEntry);
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2>
        encoder_records{};
    Lz78DynamicRangeEncoderViews encoder_views{};
    ASSERT_EQ(partition_lz78_dynamic_range_encoder_views(
                  encoder, std::as_writable_bytes(
                               std::span{encoder_records}), encoder_views),
              Lz78DynamicRangeWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), 2U);

    Lz78DynamicRangeDecoderWorkspaceRequirements decoder{};
    decoder.phrase_entry_count = 2;
    decoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    decoder.views_alignment =
        alignof(marc::dictionary::internal::Lz78PhraseEntry);
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        phrase_records{};
    Lz78DynamicRangeDecoderViews decoder_views{};
    ASSERT_EQ(partition_lz78_dynamic_range_decoder_views(
                  decoder, std::as_writable_bytes(
                               std::span{phrase_records}), decoder_views),
              Lz78DynamicRangeWorkspaceError::none);
    EXPECT_EQ(decoder_views.phrases.size(), 2U);

    auto changed = decoder;
    ++changed.phrase_entry_count;
    EXPECT_EQ(partition_lz78_dynamic_range_decoder_views(
                  changed, std::as_writable_bytes(
                               std::span{phrase_records}), decoder_views),
              Lz78DynamicRangeWorkspaceError::invalid_requirements);
    EXPECT_EQ(partition_lz78_dynamic_range_decoder_views(
                  decoder,
                  std::as_writable_bytes(std::span{phrase_records}).first(
                      decoder.views_bytes - 1), decoder_views),
              Lz78DynamicRangeWorkspaceError::too_small);
    std::vector<std::byte> misaligned(decoder.views_bytes + 1);
    EXPECT_EQ(partition_lz78_dynamic_range_decoder_views(
                  decoder, std::span<std::byte>{misaligned}.subspan(1),
                  decoder_views),
              Lz78DynamicRangeWorkspaceError::misaligned);
}

TEST(Lz78DynamicRangeProfile, MapsStableCoreErrors) {
    EXPECT_EQ(lz78_dynamic_range_profile_error_code(
                  Lz78DynamicRangeProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lz78_dynamic_range_profile_error_code(
                  Lz78DynamicRangeProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lz78_dynamic_range_profile_error_code(
                  Lz78DynamicRangeProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lz78_dynamic_range_profile_error_code(
                  Lz78DynamicRangeProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(lz78_dynamic_range_profile_error_code(
                  Lz78DynamicRangeProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);
}

} // namespace

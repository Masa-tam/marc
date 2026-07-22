#include "frame/lz77_dynamic_range_profile.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::Lz77DynamicRangeProfileError;

TEST(Lz77DynamicRangeProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77DynamicRangeEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77DynamicRangeProfileConfig config{2'500'000};
    ASSERT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  config, {}, stream, workspace),
              Lz77DynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lz77);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 1'048'576U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 2'097'229U);
}

TEST(Lz77DynamicRangeProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77DynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  {17}, {}, stream, workspace),
              Lz77DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 272U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 621U);

    workspace = {1, 1, 1};
    ASSERT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  {}, {}, stream, workspace),
              Lz77DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77DynamicRangeProfile, RejectsUnsupportedWorstCasePayload) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77DynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 548;
    EXPECT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  {17}, limits, stream, workspace),
              Lz77DynamicRangeProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77DynamicRangeProfile, EnforcesFormatFrameCap) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77DynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 2U << 20;
    const marc::frame::Lz77DynamicRangeProfileConfig config{
        (1U << 20) + 1U, (1U << 20) + 1U, {}};
    EXPECT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  config, limits, stream, workspace),
              Lz77DynamicRangeProfileError::limit_exceeded);
}

TEST(Lz77DynamicRangeProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77DynamicRangeEncoderWorkspaceRequirements workspace{};
    marc::frame::Lz77DynamicRangeProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lz77_dynamic_range_profile(
                  config, {}, stream, workspace),
              Lz77DynamicRangeProfileError::invalid_configuration);
}

TEST(Lz77DynamicRangeProfile,
     DecoderWorkspaceUsesOnlyLocalLimitsAndProfileCap) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::Lz77DynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lz77_dynamic_range_decoder_workspace(
            limits, workspace),
        Lz77DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(Lz77DynamicRangeProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lz77_dynamic_range_profile_error_code;
    EXPECT_EQ(lz77_dynamic_range_profile_error_code(
                  Lz77DynamicRangeProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lz77_dynamic_range_profile_error_code(
                  Lz77DynamicRangeProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lz77_dynamic_range_profile_error_code(
                  Lz77DynamicRangeProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lz77_dynamic_range_profile_error_code(
                  Lz77DynamicRangeProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lz77_dynamic_range_profile_error_code(
                  Lz77DynamicRangeProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

#include "frame/lzss_dynamic_range_profile.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::LzssDynamicRangeProfileError;

TEST(LzssDynamicRangeProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzssDynamicRangeProfileConfig config{2'500'000};
    ASSERT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  config, {}, stream, workspace),
              LzssDynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzss);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 131'072U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 262'221U);
}

TEST(LzssDynamicRangeProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  {17}, {}, stream, workspace),
              LzssDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 34U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 145U);

    workspace = {1, 1, 1};
    ASSERT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  {}, {}, stream, workspace),
              LzssDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssDynamicRangeProfile, RejectsUnsupportedWorstCasePayload) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 72;
    EXPECT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  {17}, limits, stream, workspace),
              LzssDynamicRangeProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssDynamicRangeProfile, EnforcesAggregateWorkspaceBound) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 32;
    limits.max_block_size = 32;
    limits.max_internal_buffered_bytes = 195;
    EXPECT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  {17}, limits, stream, workspace),
              LzssDynamicRangeProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssDynamicRangeProfile, EnforcesFormatFrameCap) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT64_C(16) << 20;
    const marc::frame::LzssDynamicRangeProfileConfig config{
        (UINT32_C(1) << 23) + 1U, (UINT32_C(1) << 23) + 1U, {}};
    EXPECT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  config, limits, stream, workspace),
              LzssDynamicRangeProfileError::limit_exceeded);
}

TEST(LzssDynamicRangeProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements workspace{};
    marc::frame::LzssDynamicRangeProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lzss_dynamic_range_profile(
                  config, {}, stream, workspace),
              LzssDynamicRangeProfileError::invalid_configuration);
}

TEST(LzssDynamicRangeProfile,
     DecoderWorkspaceUsesOnlyLocalLimitsAndProfileCap) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::LzssDynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzss_dynamic_range_decoder_workspace(
            limits, workspace),
        LzssDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(LzssDynamicRangeProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lzss_dynamic_range_profile_error_code;
    EXPECT_EQ(lzss_dynamic_range_profile_error_code(
                  LzssDynamicRangeProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_dynamic_range_profile_error_code(
                  LzssDynamicRangeProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_dynamic_range_profile_error_code(
                  LzssDynamicRangeProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_dynamic_range_profile_error_code(
                  LzssDynamicRangeProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_dynamic_range_profile_error_code(
                  LzssDynamicRangeProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

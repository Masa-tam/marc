#include "frame/dynamic_range_profile.hpp"
#include "entropy/dynamic_range_format.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::DynamicRangeProfileError;

TEST(DynamicRangeProfile, NormalizesVersionOneAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::DynamicRangeEncoderWorkspaceRequirements workspace{};
    const marc::frame::DynamicRangeProfileConfig config{2'500'000, 1'000'000};
    ASSERT_EQ(marc::frame::make_dynamic_range_profile(
                  config, {}, stream, workspace),
              DynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::none);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(workspace.frame_input_bytes, 1'000'000U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              56U + 16U + 2U * 1'000'000U + 5U);
}

TEST(DynamicRangeProfile, UsesActualLargestShortFrame) {
    marc::frame::StreamHeader stream{};
    marc::frame::DynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_dynamic_range_profile(
                  {17, 1U << 20}, {}, stream, workspace),
              DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 2U * 17U + 5U);
}

TEST(DynamicRangeProfile, EmptyStreamNeedsNoFrameWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::DynamicRangeEncoderWorkspaceRequirements workspace{1, 1};
    ASSERT_EQ(marc::frame::make_dynamic_range_profile(
                  {}, {}, stream, workspace),
              DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(DynamicRangeProfile, RejectsUnsupportedWorstCaseCapacity) {
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 8;
    marc::frame::StreamHeader stream{};
    marc::frame::DynamicRangeEncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::make_dynamic_range_profile(
                  {2, 2}, limits, stream, workspace),
              DynamicRangeProfileError::limit_exceeded);
}

TEST(DynamicRangeProfile, RequiresTheVariantModelTotal) {
    marc::core::DecoderLimits limits{};
    limits.max_range_model_total =
        marc::entropy::internal::dynamic_range_model_total_limit - 1U;
    marc::frame::StreamHeader stream{};
    marc::frame::DynamicRangeEncoderWorkspaceRequirements encoder{};
    EXPECT_EQ(marc::frame::make_dynamic_range_profile(
                  {1, 1}, limits, stream, encoder),
              DynamicRangeProfileError::limit_exceeded);
    marc::frame::DynamicRangeDecoderWorkspaceRequirements decoder{};
    EXPECT_EQ(marc::frame::calculate_dynamic_range_decoder_workspace(
                  limits, decoder),
              DynamicRangeProfileError::limit_exceeded);
}

TEST(DynamicRangeProfile, DecoderWorkspaceComesFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::DynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_dynamic_range_decoder_workspace(
                  limits, workspace),
              DynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(DynamicRangeProfile, MapsStableErrorCategories) {
    using marc::core::ErrorCode;
    EXPECT_EQ(marc::frame::dynamic_range_profile_error_code(
                  DynamicRangeProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::dynamic_range_profile_error_code(
                  DynamicRangeProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::dynamic_range_profile_error_code(
                  DynamicRangeProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

#include "entropy/tans_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/tans_profile.hpp"

#include <gtest/gtest.h>

TEST(TansProfile, BuildsCanonicalProfileAndWorkspaceBound) {
    marc::frame::StreamHeader stream{};
    marc::frame::TansEncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::make_tans_profile({7, 4, 2}, {}, stream, workspace),
              marc::frame::TansProfileError::none);
    EXPECT_EQ(stream.entropy_algorithm, marc::frame::EntropyAlgorithm::tans);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.entropy_block_size, 2U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              marc::frame::frame_header_size
                  + 2U * marc::entropy::internal::tans_descriptor_size
                  + 2U * (marc::entropy::internal::tans_min_payload_size + 3U));
}

TEST(TansProfile, HandlesEmptyAndRejectsInvalidOrLimitedPolicy) {
    marc::frame::StreamHeader stream{};
    marc::frame::TansEncoderWorkspaceRequirements workspace{1, 1};
    EXPECT_EQ(marc::frame::make_tans_profile({0, 4, 2}, {}, stream, workspace),
              marc::frame::TansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, marc::frame::frame_header_size);
    EXPECT_EQ(marc::frame::make_tans_profile({1, 0, 2}, {}, stream, workspace),
              marc::frame::TansProfileError::invalid_configuration);
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(marc::frame::make_tans_profile({4, 4, 2}, limits,
                                             stream, workspace),
              marc::frame::TansProfileError::limit_exceeded);
    EXPECT_EQ(marc::frame::tans_profile_error_code(
                  marc::frame::TansProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);
}

TEST(TansProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 123;
    limits.max_block_size = 123;
    limits.max_internal_buffered_bytes = 456;
    limits.max_blocks_per_frame = 7;
    marc::frame::TansDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::calculate_tans_decoder_workspace(limits, workspace),
              marc::frame::TansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              marc::frame::frame_header_size + 456U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 123U);
    EXPECT_EQ(workspace.block_view_count, 7U);
}

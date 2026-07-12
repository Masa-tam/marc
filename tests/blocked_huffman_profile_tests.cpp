#include "frame/blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using marc::frame::ProfileError;

TEST(BlockedHuffmanProfile, NormalizesVersionOneConfiguration) {
    marc::frame::StreamHeader stream{};
    marc::frame::EncoderWorkspaceRequirements workspace{};
    const marc::frame::BlockedHuffmanProfileConfig config{
        2'500'000, 1'000'000, 65'536};

    EXPECT_EQ(marc::frame::make_blocked_huffman_profile(
                  config, {}, stream, workspace), ProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::none);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(stream.entropy_variant, 1);
    EXPECT_EQ(stream.original_size, config.original_size);
    EXPECT_EQ(workspace.frame_input_bytes, 1'000'000U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              56U + 1'000'000U + 16U * 16U);
}

TEST(BlockedHuffmanProfile, UsesTheActualLargestShortFrame) {
    marc::frame::StreamHeader stream{};
    marc::frame::EncoderWorkspaceRequirements workspace{};
    const marc::frame::BlockedHuffmanProfileConfig config{17, 1U << 20, 8};

    ASSERT_EQ(marc::frame::make_blocked_huffman_profile(
                  config, {}, stream, workspace), ProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 17U + 3U * 16U);
}

TEST(BlockedHuffmanProfile, EmptyInputNeedsNoFrameWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::EncoderWorkspaceRequirements workspace{};

    ASSERT_EQ(marc::frame::make_blocked_huffman_profile(
                  {}, {}, stream, workspace), ProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(BlockedHuffmanProfile, RejectsLimitsBeforeReturningRequirements) {
    marc::frame::StreamHeader stream{};
    marc::frame::EncoderWorkspaceRequirements workspace{11, 12};
    const marc::frame::BlockedHuffmanProfileConfig config{100, 100, 1};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 99;

    EXPECT_EQ(marc::frame::make_blocked_huffman_profile(
                  config, limits, stream, workspace),
              ProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(BlockedHuffmanProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_internal_buffered_bytes = 8192;
    limits.max_blocks_per_frame = 7;
    marc::frame::DecoderWorkspaceRequirements workspace{};

    ASSERT_EQ(marc::frame::calculate_blocked_huffman_decoder_workspace(
                  limits, workspace), ProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
    EXPECT_EQ(workspace.block_view_count, 7U);
}

TEST(BlockedHuffmanProfile, MapsErrorsToStableCoreCategories) {
    using marc::core::ErrorCode;
    EXPECT_EQ(marc::frame::profile_error_code(ProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(marc::frame::profile_error_code(
                  ProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::profile_error_code(ProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::profile_error_code(ProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(marc::frame::profile_error_code(
                  ProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

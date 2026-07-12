#include "frame/adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::AdaptiveHuffmanProfileError;

TEST(AdaptiveHuffmanProfile, NormalizesVersionOneAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::AdaptiveHuffmanProfileConfig config{2'500'000, 1'000'000};
    ASSERT_EQ(marc::frame::make_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::none);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(workspace.frame_input_bytes, 1'000'000U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              56U + 16U + 33U * 1'000'000U);
}

TEST(AdaptiveHuffmanProfile, UsesActualLargestShortFrame) {
    marc::frame::StreamHeader stream{};
    marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_adaptive_huffman_profile(
                  {17, 1U << 20}, {}, stream, workspace),
              AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 33U * 17U);
}

TEST(AdaptiveHuffmanProfile, EmptyStreamNeedsNoFrameWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements workspace{1, 1};
    ASSERT_EQ(marc::frame::make_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(AdaptiveHuffmanProfile, RejectsUnsupportedWorstCaseCapacity) {
    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 32;
    marc::frame::StreamHeader stream{};
    marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::make_adaptive_huffman_profile(
                  {2, 2}, limits, stream, workspace),
              AdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(AdaptiveHuffmanProfile, DecoderWorkspaceComesFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::AdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_adaptive_huffman_decoder_workspace(
                  limits, workspace),
              AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(AdaptiveHuffmanProfile, MapsStableErrorCategories) {
    using marc::core::ErrorCode;
    EXPECT_EQ(marc::frame::adaptive_huffman_profile_error_code(
                  AdaptiveHuffmanProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::adaptive_huffman_profile_error_code(
                  AdaptiveHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::adaptive_huffman_profile_error_code(
                  AdaptiveHuffmanProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

#include "frame/lz77_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::Lz77AdaptiveHuffmanProfileError;

TEST(Lz77AdaptiveHuffmanProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77AdaptiveHuffmanProfileConfig config{2'500'000};
    ASSERT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lz77);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 1'048'576U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 34'603'080U);
}

TEST(Lz77AdaptiveHuffmanProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  {17}, {}, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 272U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 9'048U);

    workspace = {1, 1, 1};
    ASSERT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77AdaptiveHuffmanProfile, RejectsUnsupportedWorstCasePayload) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77AdaptiveHuffmanProfileConfig config{
        1U << 20, 1U << 20, {}};
    EXPECT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77AdaptiveHuffmanProfile, EnforcesFormatFrameCap) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 2U << 20;
    const marc::frame::Lz77AdaptiveHuffmanProfileConfig config{
        (1U << 20) + 1U, (1U << 20) + 1U, {}};
    EXPECT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  config, limits, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(Lz77AdaptiveHuffmanProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::Lz77AdaptiveHuffmanProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lz77_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              Lz77AdaptiveHuffmanProfileError::invalid_configuration);
}

TEST(Lz77AdaptiveHuffmanProfile, DecoderWorkspaceUsesOnlyLocalLimitsAndProfileCap) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::Lz77AdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lz77_adaptive_huffman_decoder_workspace(
            limits, workspace),
        Lz77AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(Lz77AdaptiveHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lz77_adaptive_huffman_profile_error_code;
    EXPECT_EQ(lz77_adaptive_huffman_profile_error_code(
                  Lz77AdaptiveHuffmanProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lz77_adaptive_huffman_profile_error_code(
                  Lz77AdaptiveHuffmanProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lz77_adaptive_huffman_profile_error_code(
                  Lz77AdaptiveHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lz77_adaptive_huffman_profile_error_code(
                  Lz77AdaptiveHuffmanProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lz77_adaptive_huffman_profile_error_code(
                  Lz77AdaptiveHuffmanProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

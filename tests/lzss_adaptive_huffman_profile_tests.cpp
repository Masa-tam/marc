#include "frame/lzss_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

namespace {

using marc::frame::LzssAdaptiveHuffmanProfileError;

TEST(LzssAdaptiveHuffmanProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzssAdaptiveHuffmanProfileConfig config{2'500'000};
    ASSERT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzssAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzss);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 131'072U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 4'325'448U);
}

TEST(LzssAdaptiveHuffmanProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  {17}, {}, stream, workspace),
              LzssAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 34U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 1'194U);

    workspace = {1, 1, 1};
    ASSERT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              LzssAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssAdaptiveHuffmanProfile, RejectsUnsupportedWorstCasePayload) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzssAdaptiveHuffmanProfileConfig config{
        1U << 20, 1U << 20, {}};
    EXPECT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzssAdaptiveHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssAdaptiveHuffmanProfile, EnforcesFormatFrameCap) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 2U << 20;
    const marc::frame::LzssAdaptiveHuffmanProfileConfig config{
        (1U << 20) + 1U, (1U << 20) + 1U, {}};
    EXPECT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  config, limits, stream, workspace),
              LzssAdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(LzssAdaptiveHuffmanProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzssAdaptiveHuffmanProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lzss_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzssAdaptiveHuffmanProfileError::invalid_configuration);
}

TEST(LzssAdaptiveHuffmanProfile, DecoderWorkspaceUsesOnlyLocalLimitsAndProfileCap) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    marc::frame::LzssAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzss_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzssAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
}

TEST(LzssAdaptiveHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lzss_adaptive_huffman_profile_error_code;
    EXPECT_EQ(lzss_adaptive_huffman_profile_error_code(
                  LzssAdaptiveHuffmanProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_adaptive_huffman_profile_error_code(
                  LzssAdaptiveHuffmanProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_adaptive_huffman_profile_error_code(
                  LzssAdaptiveHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_adaptive_huffman_profile_error_code(
                  LzssAdaptiveHuffmanProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_adaptive_huffman_profile_error_code(
                  LzssAdaptiveHuffmanProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

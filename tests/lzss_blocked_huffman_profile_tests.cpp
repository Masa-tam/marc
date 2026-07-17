#include "frame/lzss_blocked_huffman_profile.hpp"
#include "frame/lzss_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_blocked_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::frame::LzssBlockedHuffmanProfileError;

TEST(LzssBlockedHuffmanProfile, BuildsCanonicalProfileAndWorstCaseWorkspaces) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzssBlockedHuffmanProfileConfig config{
        2'500'000, 1'000'000, 65'536, {}};
    ASSERT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzssBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzss);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 1'000'000U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 2'000'000U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              56U + 2'000'000U + 31U * 16U);
}

TEST(LzssBlockedHuffmanProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzssBlockedHuffmanProfileConfig short_config{
        17, 1U << 20, 64, {}};
    ASSERT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  short_config, {}, stream, workspace),
              LzssBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 34U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 106U);

    ASSERT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              LzssBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(LzssBlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 3;
    const marc::frame::LzssBlockedHuffmanProfileConfig too_many_blocks{
        2, 2, 1, {}};
    EXPECT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  too_many_blocks, limits, stream, workspace),
              LzssBlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);

    limits = {};
    limits.max_block_size = 4;
    limits.max_internal_buffered_bytes = 81;
    const marc::frame::LzssBlockedHuffmanProfileConfig aggregate_limited{
        2, 2, 4, {}};
    EXPECT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  aggregate_limited, limits, stream, workspace),
              LzssBlockedHuffmanProfileError::limit_exceeded);
}

TEST(LzssBlockedHuffmanProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzssBlockedHuffmanProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzssBlockedHuffmanProfileError::invalid_configuration);
}

TEST(LzssBlockedHuffmanProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    limits.max_blocks_per_frame = 7;
    marc::frame::LzssBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzss_blocked_huffman_decoder_workspace(
            limits, workspace),
        LzssBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
    EXPECT_EQ(workspace.block_view_count, 7U);
}

TEST(LzssBlockedHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lzss_blocked_huffman_profile_error_code;
    EXPECT_EQ(lzss_blocked_huffman_profile_error_code(
                  LzssBlockedHuffmanProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lzss_blocked_huffman_profile_error_code(
                  LzssBlockedHuffmanProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lzss_blocked_huffman_profile_error_code(
                  LzssBlockedHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lzss_blocked_huffman_profile_error_code(
                  LzssBlockedHuffmanProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lzss_blocked_huffman_profile_error_code(
                  LzssBlockedHuffmanProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

TEST(LzssBlockedHuffmanProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 2;
    limits.max_block_size = 4;
    limits.max_compressed_payload_size = 4;
    limits.max_dictionary_serialized_size = 4;
    limits.max_internal_buffered_bytes = 200;
    limits.max_blocks_per_frame = 1;
    const marc::frame::LzssBlockedHuffmanProfileConfig config{
        raw.size(), 2, 4, {}};
    marc::frame::StreamHeader stream{};
    marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(marc::frame::make_lzss_blocked_huffman_profile(
                  config, limits, stream, encoder_ws),
              LzssBlockedHuffmanProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    marc::frame::LzssBlockedHuffmanFrameStreamingEncoder encoder{
        stream, config.parameters, limits, frame_input, encode_dictionary,
        frame_encoded};
    std::array<std::byte, 306> encoded{};
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    ASSERT_EQ(encoded_result.output_produced, encoded.size());

    marc::frame::LzssBlockedHuffmanDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(
        marc::frame::calculate_lzss_blocked_huffman_decoder_workspace(
            limits, decoder_ws),
        LzssBlockedHuffmanProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_decoded(decoder_ws.frame_decoded_bytes);
    std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
        decoder_ws.block_view_count);
    marc::frame::LzssBlockedHuffmanFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_dictionary, frame_decoded, views};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded, raw);
}

} // namespace

#include "frame/lz77_blocked_huffman_profile.hpp"
#include "frame/lz77_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lz77_blocked_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

using marc::frame::Lz77BlockedHuffmanProfileError;

TEST(Lz77BlockedHuffmanProfile, BuildsCanonicalProfileAndWorstCaseWorkspaces) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77BlockedHuffmanProfileConfig config{
        2'500'000, 1'000'000, 65'536, {}};
    ASSERT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  config, {}, stream, workspace),
              Lz77BlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lz77);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 1'000'000U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 16'000'000U);
    EXPECT_EQ(workspace.frame_encoded_bytes,
              56U + 16'000'000U + 245U * 16U);
}

TEST(Lz77BlockedHuffmanProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77BlockedHuffmanProfileConfig short_config{
        17, 1U << 20, 64, {}};
    ASSERT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  short_config, {}, stream, workspace),
              Lz77BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 272U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 408U);

    ASSERT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              Lz77BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77BlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 31;
    const marc::frame::Lz77BlockedHuffmanProfileConfig too_many_blocks{
        2, 2, 1, {}};
    EXPECT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  too_many_blocks, limits, stream, workspace),
              Lz77BlockedHuffmanProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);

    limits = {};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 153;
    const marc::frame::Lz77BlockedHuffmanProfileConfig aggregate_limited{
        2, 2, 16, {}};
    EXPECT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  aggregate_limited, limits, stream, workspace),
              Lz77BlockedHuffmanProfileError::limit_exceeded);
}

TEST(Lz77BlockedHuffmanProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::Lz77BlockedHuffmanProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  config, {}, stream, workspace),
              Lz77BlockedHuffmanProfileError::invalid_configuration);
}

TEST(Lz77BlockedHuffmanProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    limits.max_blocks_per_frame = 7;
    marc::frame::Lz77BlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lz77_blocked_huffman_decoder_workspace(
            limits, workspace),
        Lz77BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
    EXPECT_EQ(workspace.block_view_count, 7U);
}

TEST(Lz77BlockedHuffmanProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lz77_blocked_huffman_profile_error_code;
    EXPECT_EQ(lz77_blocked_huffman_profile_error_code(
                  Lz77BlockedHuffmanProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lz77_blocked_huffman_profile_error_code(
                  Lz77BlockedHuffmanProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lz77_blocked_huffman_profile_error_code(
                  Lz77BlockedHuffmanProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lz77_blocked_huffman_profile_error_code(
                  Lz77BlockedHuffmanProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lz77_blocked_huffman_profile_error_code(
                  Lz77BlockedHuffmanProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

TEST(Lz77BlockedHuffmanProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 2;
    limits.max_block_size = 16;
    limits.max_compressed_payload_size = 32;
    limits.max_dictionary_serialized_size = 32;
    limits.max_internal_buffered_bytes = 200;
    limits.max_blocks_per_frame = 2;
    const marc::frame::Lz77BlockedHuffmanProfileConfig config{
        raw.size(), 2, 16, {}};
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77BlockedHuffmanEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(marc::frame::make_lz77_blocked_huffman_profile(
                  config, limits, stream, encoder_ws),
              Lz77BlockedHuffmanProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    marc::frame::Lz77BlockedHuffmanFrameStreamingEncoder encoder{
        stream, config.parameters, limits, frame_input, encode_dictionary,
        frame_encoded};
    std::array<std::byte, 408> encoded{};
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    ASSERT_EQ(encoded_result.output_produced, encoded.size());

    marc::frame::Lz77BlockedHuffmanDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(
        marc::frame::calculate_lz77_blocked_huffman_decoder_workspace(
            limits, decoder_ws),
        Lz77BlockedHuffmanProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_decoded(decoder_ws.frame_decoded_bytes);
    std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
        decoder_ws.block_view_count);
    marc::frame::Lz77BlockedHuffmanFrameStreamingDecoder decoder{
        limits, decode_encoded, decode_dictionary, frame_decoded, views};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded, raw);
}

} // namespace

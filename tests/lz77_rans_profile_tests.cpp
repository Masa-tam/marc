#include "frame/lz77_rans_profile.hpp"
#include "frame/lz77_rans_frame_streaming_decoder.hpp"
#include "frame/lz77_rans_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz77RansProfileError;

TEST(Lz77RansProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77RansEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77RansProfileConfig config{2'500'000};
    ASSERT_EQ(marc::frame::make_lz77_rans_profile(
                  config, {}, stream, workspace),
              Lz77RansProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lz77);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::rans);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 65'536U);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 1'048'576U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 1'057'208U);
}

TEST(Lz77RansProfile, UsesActualLargestShortFrameAndEmptyExtent) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77RansEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz77RansProfileConfig short_config{
        17, 65'536, 65'536, {}};
    ASSERT_EQ(marc::frame::make_lz77_rans_profile(
                  short_config, {}, stream, workspace),
              Lz77RansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 272U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 864U);

    ASSERT_EQ(marc::frame::make_lz77_rans_profile(
                  {}, {}, stream, workspace),
              Lz77RansProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77RansProfile, EnforcesBlockPayloadAggregateAndFrameLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77RansEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 31;
    const marc::frame::Lz77RansProfileConfig too_many_blocks{
        2, 2, 1, {}};
    EXPECT_EQ(marc::frame::make_lz77_rans_profile(
                  too_many_blocks, limits, stream, workspace),
              Lz77RansProfileError::limit_exceeded);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);

    limits = {};
    limits.max_compressed_payload_size = 279;
    const marc::frame::Lz77RansProfileConfig payload_limited{
        17, 65'536, 65'536, {}};
    EXPECT_EQ(marc::frame::make_lz77_rans_profile(
                  payload_limited, limits, stream, workspace),
              Lz77RansProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 624;
    const marc::frame::Lz77RansProfileConfig aggregate_limited{
        1, 1, 16, {}};
    EXPECT_EQ(marc::frame::make_lz77_rans_profile(
                  aggregate_limited, limits, stream, workspace),
              Lz77RansProfileError::limit_exceeded);

    limits = {};
    limits.max_frame_size = 2U << 20;
    const marc::frame::Lz77RansProfileConfig oversized_frame{
        (1U << 20) + 1U, (1U << 20) + 1U, 65'536, {}};
    EXPECT_EQ(marc::frame::make_lz77_rans_profile(
                  oversized_frame, limits, stream, workspace),
              Lz77RansProfileError::limit_exceeded);
}

TEST(Lz77RansProfile, RejectsInvalidDictionaryParameters) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77RansEncoderWorkspaceRequirements workspace{};
    marc::frame::Lz77RansProfileConfig config{};
    config.parameters.min_match_length = 2;
    config.parameters.max_match_length = 1;
    EXPECT_EQ(marc::frame::make_lz77_rans_profile(
                  config, {}, stream, workspace),
              Lz77RansProfileError::invalid_configuration);
}

TEST(Lz77RansProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 8192;
    limits.max_blocks_per_frame = 7;
    marc::frame::Lz77RansDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lz77_rans_decoder_workspace(
                  limits, workspace),
              Lz77RansProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4096U);
    EXPECT_EQ(workspace.block_view_count, 7U);

    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(marc::frame::calculate_lz77_rans_decoder_workspace(
                  limits, workspace),
              Lz77RansProfileError::arithmetic_overflow);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
}

TEST(Lz77RansProfile, MapsStableCoreErrors) {
    using marc::core::ErrorCode;
    using marc::frame::lz77_rans_profile_error_code;
    EXPECT_EQ(lz77_rans_profile_error_code(Lz77RansProfileError::none),
              ErrorCode::none);
    EXPECT_EQ(lz77_rans_profile_error_code(
                  Lz77RansProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(lz77_rans_profile_error_code(
                  Lz77RansProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(lz77_rans_profile_error_code(
                  Lz77RansProfileError::limit_exceeded),
              ErrorCode::limit_exceeded);
    EXPECT_EQ(lz77_rans_profile_error_code(
                  Lz77RansProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

TEST(Lz77RansProfile, RequirementsConstructStreamingRoundTrip) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'X'}};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_dictionary_serialized_size = 6000;
    limits.max_internal_buffered_bytes = 16'384;
    limits.max_blocks_per_frame = 7;
    const marc::frame::Lz77RansProfileConfig config{
        raw.size(), 2, 5, {}};
    marc::frame::StreamHeader stream{};
    marc::frame::Lz77RansEncoderWorkspaceRequirements encoder_ws{};
    ASSERT_EQ(marc::frame::make_lz77_rans_profile(
                  config, limits, stream, encoder_ws),
              Lz77RansProfileError::none);
    std::vector<std::byte> frame_input(encoder_ws.frame_input_bytes);
    std::vector<std::byte> encode_dictionary(
        encoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_encoded(encoder_ws.frame_encoded_bytes);
    marc::frame::Lz77RansFrameStreamingEncoder encoder{
        stream, config.parameters, limits, frame_input, encode_dictionary,
        frame_encoded};
    std::vector<std::byte> encoded(16'384);
    const auto encoded_result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(encoded_result.status, marc::core::StreamStatus::end_of_stream);
    ASSERT_EQ(encoded_result.input_consumed, raw.size());
    encoded.resize(encoded_result.output_produced);

    marc::frame::Lz77RansDecoderWorkspaceRequirements decoder_ws{};
    ASSERT_EQ(marc::frame::calculate_lz77_rans_decoder_workspace(
                  limits, decoder_ws),
              Lz77RansProfileError::none);
    std::vector<std::byte> decode_encoded(decoder_ws.frame_encoded_bytes);
    std::vector<std::byte> decode_dictionary(
        decoder_ws.dictionary_staging_bytes);
    std::vector<std::byte> frame_decoded(decoder_ws.frame_decoded_bytes);
    std::vector<marc::entropy::internal::RansBlockView> views(
        decoder_ws.block_view_count);
    marc::frame::Lz77RansFrameStreamingDecoder decoder{
        limits, decode_encoded, views, decode_dictionary, frame_decoded};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result = decoder.process(
        encoded, decoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(decoded_result.error.code, marc::core::ErrorCode::none);
    ASSERT_EQ(decoded_result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoded_result.input_consumed, encoded.size());
    EXPECT_EQ(decoded_result.output_produced, raw.size());
    EXPECT_EQ(decoded, raw);
}

} // namespace

#include "frame/checksum_raw_profile.hpp"
#include "frame/checksum_raw_streaming_encoder.hpp"

#include "core/crc32c.hpp"
#include "frame/frame_checksum.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {

using marc::frame::ChecksumRawProfileError;
using marc::frame::ChecksumRawWorkspaceRequirements;

TEST(ChecksumRawProfile, BuildsCanonicalVersionOneOneProfile) {
    marc::frame::StreamHeader stream{};
    marc::frame::HashDescriptor descriptor{};
    ChecksumRawWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_checksum_raw_profile_v1_1(
                  {7, 4}, {}, stream, descriptor, workspace),
              ChecksumRawProfileError::none);
    EXPECT_EQ(stream.minor_version,
              marc::frame::hash_format_minor_version);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::none);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::none);
    EXPECT_EQ(stream.hash_descriptors_size,
              marc::frame::hash_descriptor_size);
    EXPECT_EQ(stream.original_size, 7U);
    EXPECT_EQ(descriptor.algorithm_id,
              marc::core::crc32c_algorithm_id);
    EXPECT_EQ(descriptor.target,
              marc::frame::HashTarget::uncompressed_bytes);
    EXPECT_EQ(descriptor.scope, marc::frame::HashScope::per_frame);
    EXPECT_EQ(descriptor.digest_size, 4U);
    EXPECT_EQ(workspace.serialized_frame_bytes, 56U + 4U + 4U);

    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'F'}, std::byte{'G'}};
    std::array<std::byte, 64> frame_workspace{};
    std::array<std::byte, 207> encoded{};
    marc::frame::ChecksumRawStreamingEncoder encoder{
        stream, descriptor, {}, frame_workspace};
    const auto result = encoder.process(
        input, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded.size());
}

TEST(ChecksumRawProfile, UsesActualLargestShortFrame) {
    marc::frame::StreamHeader stream{};
    marc::frame::HashDescriptor descriptor{};
    ChecksumRawWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_checksum_raw_profile_v1_1(
                  {17, 1024}, {}, stream, descriptor, workspace),
              ChecksumRawProfileError::none);
    EXPECT_EQ(workspace.serialized_frame_bytes, 56U + 17U + 4U);
}

TEST(ChecksumRawProfile, EmptyStreamNeedsNoFrameWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::HashDescriptor descriptor{};
    ChecksumRawWorkspaceRequirements workspace{999};
    ASSERT_EQ(marc::frame::make_checksum_raw_profile_v1_1(
                  {}, {}, stream, descriptor, workspace),
              ChecksumRawProfileError::none);
    EXPECT_EQ(workspace.serialized_frame_bytes, 0U);
    EXPECT_EQ(descriptor.algorithm_id,
              marc::core::crc32c_algorithm_id);
}

TEST(ChecksumRawProfile, RejectsConfigurationAndAggregateLimitsAtomically) {
    marc::frame::StreamHeader stream{};
    stream.original_size = 999;
    marc::frame::HashDescriptor descriptor{};
    descriptor.algorithm_id = 999;
    ChecksumRawWorkspaceRequirements workspace{999};
    EXPECT_EQ(marc::frame::make_checksum_raw_profile_v1_1(
                  {1, 0}, {}, stream, descriptor, workspace),
              ChecksumRawProfileError::invalid_configuration);
    EXPECT_EQ(stream.original_size, 0U);
    EXPECT_EQ(descriptor.algorithm_id, 0U);
    EXPECT_EQ(workspace.serialized_frame_bytes, 0U);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 63;
    limits.max_block_size = 63;
    EXPECT_EQ(marc::frame::make_checksum_raw_profile_v1_1(
                  {4, 4}, limits, stream, descriptor, workspace),
              ChecksumRawProfileError::limit_exceeded);
    EXPECT_EQ(stream.original_size, 0U);
    EXPECT_EQ(descriptor.algorithm_id, 0U);
    EXPECT_EQ(workspace.serialized_frame_bytes, 0U);
}

TEST(ChecksumRawProfile, DecoderWorkspaceDependsOnlyOnLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 4096;
    limits.max_block_size = 4096;
    limits.max_compressed_payload_size = 2048;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_internal_buffered_bytes = 8192;
    ChecksumRawWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_checksum_raw_decoder_workspace_v1_1(
                  limits, workspace),
              ChecksumRawProfileError::none);
    EXPECT_EQ(workspace.serialized_frame_bytes, 56U + 1024U + 4U);

    limits.max_internal_buffered_bytes = 512;
    limits.max_block_size = 512;
    ASSERT_EQ(marc::frame::calculate_checksum_raw_decoder_workspace_v1_1(
                  limits, workspace),
              ChecksumRawProfileError::none);
    EXPECT_EQ(workspace.serialized_frame_bytes, 512U);
}

TEST(ChecksumRawProfile, RejectsInvalidLimitsAndMapsStableErrors) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    ChecksumRawWorkspaceRequirements workspace{999};
    EXPECT_EQ(marc::frame::calculate_checksum_raw_decoder_workspace_v1_1(
                  limits, workspace),
              ChecksumRawProfileError::invalid_configuration);
    EXPECT_EQ(workspace.serialized_frame_bytes, 0U);

    using marc::core::ErrorCode;
    EXPECT_EQ(marc::frame::checksum_raw_profile_error_code(
                  ChecksumRawProfileError::invalid_configuration),
              ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::checksum_raw_profile_error_code(
                  ChecksumRawProfileError::unsupported),
              ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::checksum_raw_profile_error_code(
                  ChecksumRawProfileError::arithmetic_overflow),
              ErrorCode::limit_exceeded);
}

} // namespace

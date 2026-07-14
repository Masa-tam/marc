#include "frame/lzd_profile.hpp"

#include "dictionary/lzd_encoder.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {
using namespace marc::frame;

TEST(LzdProfile, BuildsCanonicalNoneProfileAndEncoderWorkspace) {
    LzdProfileConfig config{};
    config.original_size = 10;
    config.frame_size = 4;
    StreamHeader stream{};
    LzdEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzd_profile(config, {}, stream, workspace),
              LzdProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzd);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm, EntropyAlgorithm::none);
    EXPECT_EQ(stream.entropy_variant, 0U);
    EXPECT_EQ(stream.frame_size, 4U);
    EXPECT_EQ(stream.dictionary_parameters_size,
              marc::dictionary::internal::lzd_parameter_size);
    EXPECT_EQ(stream.original_size, 10U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 16U);
    EXPECT_EQ(workspace.dictionary_entries, 2U);
}

TEST(LzdProfile, HandlesEmptyStreamAndFinalShortFrameBound) {
    LzdProfileConfig config{};
    config.original_size = 0;
    StreamHeader stream{};
    LzdEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lzd_profile(config, {}, stream, workspace),
              LzdProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size);
    EXPECT_EQ(workspace.dictionary_entries, 0U);

    config.original_size = 3;
    config.frame_size = 8;
    ASSERT_EQ(make_lzd_profile(config, {}, stream, workspace),
              LzdProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 3U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 16U);
    EXPECT_EQ(workspace.dictionary_entries, 1U);
}

TEST(LzdProfile, RejectsInvalidConfigurationAndCoupledEncoderLimits) {
    StreamHeader stream{};
    LzdEncoderWorkspaceRequirements workspace{};
    LzdProfileConfig config{};
    config.original_size = 4;
    config.frame_size = 0;
    EXPECT_EQ(make_lzd_profile(config, {}, stream, workspace),
              LzdProfileError::invalid_configuration);

    config.frame_size = 4;
    config.parameters.maximum_entries = 0;
    EXPECT_EQ(make_lzd_profile(config, {}, stream, workspace),
              LzdProfileError::invalid_configuration);

    config.parameters = {};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 15;
    EXPECT_EQ(make_lzd_profile(config, limits, stream, workspace),
              LzdProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        4 + frame_header_size + 16
        + 2 * sizeof(marc::dictionary::internal::LzdEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(make_lzd_profile(config, limits, stream, workspace),
              LzdProfileError::limit_exceeded);
}

TEST(LzdProfile, DecoderWorkspaceComesOnlyFromCoupledLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_compressed_payload_size = 2048;
    limits.max_dictionary_entries = 10;
    limits.max_internal_buffered_bytes = 4096;
    limits.max_block_size = 4096;
    LzdDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lzd_decoder_workspace(limits, workspace),
              LzdProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 1024U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.phrase_entries, 10U);
    EXPECT_EQ(workspace.expansion_entries, 11U);

    limits.max_dictionary_serialized_size = 1000;
    limits.max_compressed_payload_size = 1000;
    limits.max_dictionary_entries = 1000;
    limits.max_internal_buffered_bytes = 300;
    limits.max_block_size = 300;
    ASSERT_EQ(calculate_lzd_decoder_workspace(limits, workspace),
              LzdProfileError::none);
    std::size_t expected_payload{};
    for (std::size_t payload = 0;
         frame_header_size + payload + limits.max_frame_size
                 + (payload / marc::dictionary::internal::lzd_token_size)
                     * sizeof(marc::dictionary::internal::LzdPhraseEntry)
                 + (payload / marc::dictionary::internal::lzd_token_size + 1)
                     * sizeof(std::uint32_t)
             <= limits.max_internal_buffered_bytes;
         ++payload) {
        expected_payload = payload;
    }
    EXPECT_EQ(workspace.frame_encoded_bytes,
              frame_header_size + expected_payload);
    EXPECT_EQ(workspace.phrase_entries,
              expected_payload / marc::dictionary::internal::lzd_token_size);
    EXPECT_EQ(workspace.expansion_entries,
              workspace.phrase_entries + 1);

    limits = {};
    limits.max_frame_size = 64;
    limits.max_internal_buffered_bytes =
        frame_header_size + limits.max_frame_size
        + sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(calculate_lzd_decoder_workspace(limits, workspace),
              LzdProfileError::limit_exceeded);
}

TEST(LzdProfile, MapsErrorsAndRejectsInvalidLimitsOrHostOverflow) {
    EXPECT_EQ(lzd_profile_error_code(LzdProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lzd_profile_error_code(
                  LzdProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lzd_profile_error_code(LzdProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lzd_profile_error_code(LzdProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    LzdDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzd_decoder_workspace(limits, workspace),
              LzdProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_total_output_size =
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            + 1;
        limits.max_frame_size = limits.max_total_output_size;
        EXPECT_EQ(calculate_lzd_decoder_workspace(limits, workspace),
                  LzdProfileError::arithmetic_overflow);
    }
}

} // namespace

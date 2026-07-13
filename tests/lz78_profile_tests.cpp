#include "frame/lz78_profile.hpp"

#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_validator.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {
using namespace marc::frame;

TEST(Lz78Profile, BuildsCanonicalProfileAndWorstCaseWorkspace) {
    StreamHeader stream{};
    Lz78EncoderWorkspaceRequirements workspace{};
    Lz78ProfileConfig config{};
    config.original_size = 7;
    config.frame_size = 4;
    const auto error = make_lz78_profile(config, {}, stream, workspace);
    EXPECT_EQ(error, Lz78ProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lz78);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 4U * 8U);
    EXPECT_EQ(workspace.dictionary_entries, 4U);
}

TEST(Lz78Profile, HonorsDictionaryFreezeAndEmptyInput) {
    StreamHeader stream{};
    Lz78EncoderWorkspaceRequirements workspace{};
    Lz78ProfileConfig config{};
    config.original_size = 7;
    config.frame_size = 4;
    config.parameters.maximum_entries = 2;
    EXPECT_EQ(make_lz78_profile(config, {}, stream, workspace),
              Lz78ProfileError::none);
    EXPECT_EQ(workspace.dictionary_entries, 2U);

    EXPECT_EQ(make_lz78_profile({}, {}, stream, workspace),
              Lz78ProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size);
    EXPECT_EQ(workspace.dictionary_entries, 0U);
}

TEST(Lz78Profile, RejectsParameterAndAggregateLimits) {
    StreamHeader stream{};
    Lz78EncoderWorkspaceRequirements workspace{};
    Lz78ProfileConfig config{};
    config.original_size = 4;
    config.frame_size = 4;
    config.parameters.maximum_entries = 0;
    EXPECT_EQ(make_lz78_profile(config, {}, stream, workspace),
              Lz78ProfileError::invalid_configuration);

    config.parameters = {};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        4 + 56 + 4 * 8
        + sizeof(marc::dictionary::internal::Lz78EncoderEntry) * 4 - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(make_lz78_profile(config, limits, stream, workspace),
              Lz78ProfileError::limit_exceeded);
}

TEST(Lz78Profile, DecoderWorkspaceComesOnlyFromCoupledLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_compressed_payload_size = 2048;
    limits.max_dictionary_entries = 10;
    limits.max_internal_buffered_bytes = 4096;
    limits.max_block_size = 4096;
    Lz78DecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lz78_decoder_workspace(limits, workspace),
              Lz78ProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 1024U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.dictionary_entries, 10U);

    limits.max_dictionary_serialized_size = 1000;
    limits.max_compressed_payload_size = 1000;
    limits.max_dictionary_entries = 1000;
    limits.max_internal_buffered_bytes = 200;
    limits.max_block_size = 200;
    EXPECT_EQ(calculate_lz78_decoder_workspace(limits, workspace),
              Lz78ProfileError::none);
    std::size_t expected_payload{};
    for (std::size_t payload = 0;
         frame_header_size + payload + 1
                 + (payload / marc::dictionary::internal::lz78_token_size)
                     * sizeof(marc::dictionary::internal::Lz78PhraseEntry)
             <= limits.max_internal_buffered_bytes;
         ++payload) {
        expected_payload = payload;
    }
    EXPECT_EQ(workspace.frame_encoded_bytes,
              frame_header_size + expected_payload);
    EXPECT_EQ(workspace.dictionary_entries,
              expected_payload
                  / marc::dictionary::internal::lz78_token_size);
    const auto next_payload = expected_payload + 1;
    EXPECT_GT(frame_header_size + next_payload + 1
                  + (next_payload
                     / marc::dictionary::internal::lz78_token_size)
                      * sizeof(marc::dictionary::internal::Lz78PhraseEntry),
              limits.max_internal_buffered_bytes);
}

TEST(Lz78Profile, MapsErrorsAndRejectsInvalidLimitsOrHostOverflow) {
    EXPECT_EQ(lz78_profile_error_code(Lz78ProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lz78_profile_error_code(
                  Lz78ProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lz78_profile_error_code(Lz78ProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lz78_profile_error_code(Lz78ProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    Lz78DecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lz78_decoder_workspace(limits, workspace),
              Lz78ProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_total_output_size =
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            + 1;
        limits.max_frame_size = limits.max_total_output_size;
        EXPECT_EQ(calculate_lz78_decoder_workspace(limits, workspace),
                  Lz78ProfileError::arithmetic_overflow);
    }
}

} // namespace

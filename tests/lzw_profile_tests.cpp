#include "frame/lzw_profile.hpp"

#include "dictionary/lzw_encoder.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

namespace {
using namespace marc::frame;

TEST(LzwProfile, BuildsCanonicalProfileAndWorstCaseWorkspace) {
    StreamHeader stream{};
    LzwEncoderWorkspaceRequirements workspace{};
    LzwProfileConfig config{};
    config.original_size = 7;
    config.frame_size = 4;
    const auto error = make_lzw_profile(config, {}, stream, workspace);
    EXPECT_EQ(error, LzwProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzw);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 8U);
    EXPECT_EQ(workspace.dictionary_entries, 3U);
}

TEST(LzwProfile, HonorsDictionaryFreezeAndEmptyInput) {
    StreamHeader stream{};
    LzwEncoderWorkspaceRequirements workspace{};
    LzwProfileConfig config{};
    config.original_size = 300;
    config.frame_size = 300;
    config.parameters.maximum_code_width = 9;
    EXPECT_EQ(make_lzw_profile(config, {}, stream, workspace),
              LzwProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 338U);
    EXPECT_EQ(workspace.dictionary_entries, 256U);

    EXPECT_EQ(make_lzw_profile({}, {}, stream, workspace),
              LzwProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size);
    EXPECT_EQ(workspace.dictionary_entries, 0U);
}

TEST(LzwProfile, RejectsParameterAndAggregateLimits) {
    StreamHeader stream{};
    LzwEncoderWorkspaceRequirements workspace{};
    LzwProfileConfig config{};
    config.original_size = 4;
    config.frame_size = 4;
    config.parameters.maximum_code_width = 8;
    EXPECT_EQ(make_lzw_profile(config, {}, stream, workspace),
              LzwProfileError::invalid_configuration);

    config.parameters = {};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        4 + frame_header_size + 8
        + sizeof(marc::dictionary::internal::LzwEncoderEntry) * 3 - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(make_lzw_profile(config, limits, stream, workspace),
              LzwProfileError::limit_exceeded);
}

TEST(LzwProfile, DecoderWorkspaceComesOnlyFromCoupledLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_compressed_payload_size = 2048;
    limits.max_dictionary_entries = 1000;
    limits.max_internal_buffered_bytes = UINT64_C(1) << 20;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzwDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzw_decoder_workspace(limits, workspace),
              LzwProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 1024U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.dictionary_entries, 768U);

    limits.max_dictionary_serialized_size = 1000;
    limits.max_compressed_payload_size = 1000;
    limits.max_internal_buffered_bytes = 200;
    limits.max_block_size = 200;
    EXPECT_EQ(calculate_lzw_decoder_workspace(limits, workspace),
              LzwProfileError::none);
    std::size_t expected_payload{};
    std::size_t expected_entries{};
    for (std::size_t payload = 0; payload <= 200; ++payload) {
        const auto maximum_codes = payload / 9 * 8
            + (payload % 9 * 8) / 9;
        const auto entries = maximum_codes == 0
            ? std::size_t{0}
            : std::min<std::size_t>(maximum_codes - 1, 768);
        if (frame_header_size + payload + 1
                + entries
                    * sizeof(marc::dictionary::internal::LzwPhraseEntry)
            > limits.max_internal_buffered_bytes) {
            break;
        }
        expected_payload = payload;
        expected_entries = entries;
    }
    EXPECT_EQ(workspace.frame_encoded_bytes,
              frame_header_size + expected_payload);
    EXPECT_EQ(workspace.dictionary_entries, expected_entries);
    const auto next_payload = expected_payload + 1;
    const auto next_codes = next_payload / 9 * 8
        + (next_payload % 9 * 8) / 9;
    const auto next_entries = next_codes == 0
        ? std::size_t{0}
        : std::min<std::size_t>(next_codes - 1, 768);
    EXPECT_GT(frame_header_size + next_payload + 1
                  + next_entries
                      * sizeof(marc::dictionary::internal::LzwPhraseEntry),
              limits.max_internal_buffered_bytes);
}

TEST(LzwProfile, MapsErrorsAndRejectsInvalidLimitsOrHostOverflow) {
    EXPECT_EQ(lzw_profile_error_code(LzwProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lzw_profile_error_code(
                  LzwProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lzw_profile_error_code(LzwProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lzw_profile_error_code(LzwProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    LzwDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzw_decoder_workspace(limits, workspace),
              LzwProfileError::invalid_configuration);

    limits = {};
    limits.max_dictionary_entries = 255;
    EXPECT_EQ(calculate_lzw_decoder_workspace(limits, workspace),
              LzwProfileError::limit_exceeded);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_total_output_size =
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            + 1;
        limits.max_frame_size = limits.max_total_output_size;
        EXPECT_EQ(calculate_lzw_decoder_workspace(limits, workspace),
                  LzwProfileError::arithmetic_overflow);
    }
}

} // namespace

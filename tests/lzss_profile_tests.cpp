#include "frame/lzss_profile.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {
using namespace marc::frame;

TEST(LzssProfile, BuildsCanonicalProfileAndWorstCaseWorkspace) {
    StreamHeader stream{};
    LzssEncoderWorkspaceRequirements workspace{};
    LzssProfileConfig config{};
    config.original_size = 7;
    config.frame_size = 4;
    const auto error = make_lzss_profile(config, {}, stream, workspace);
    EXPECT_EQ(error, LzssProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lzss);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 4U * 2U);
}

TEST(LzssProfile, EmptyEncoderNeedsNoFramePayloadWorkspace) {
    StreamHeader stream{};
    LzssEncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(make_lzss_profile({}, {}, stream, workspace),
              LzssProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size);
}

TEST(LzssProfile, RejectsParameterAndAggregateLimits) {
    StreamHeader stream{};
    LzssEncoderWorkspaceRequirements workspace{};
    LzssProfileConfig config{};
    config.original_size = 4;
    config.frame_size = 4;
    config.parameters.min_match_length = 4;
    EXPECT_EQ(make_lzss_profile(config, {}, stream, workspace),
              LzssProfileError::invalid_configuration);

    config.parameters = {};
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 67;
    EXPECT_EQ(make_lzss_profile(config, limits, stream, workspace),
              LzssProfileError::limit_exceeded);
}

TEST(LzssProfile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_compressed_payload_size = 2048;
    limits.max_internal_buffered_bytes = 4096;
    limits.max_block_size = 4096;
    LzssDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzss_decoder_workspace(limits, workspace),
              LzssProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 1024U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
}

TEST(LzssProfile, RejectsInvalidLimitsAndHostOverflow) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    LzssDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lzss_decoder_workspace(limits, workspace),
              LzssProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_total_output_size =
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            + 1;
        limits.max_frame_size = limits.max_total_output_size;
        EXPECT_EQ(calculate_lzss_decoder_workspace(limits, workspace),
                  LzssProfileError::arithmetic_overflow);
    }
}

} // namespace

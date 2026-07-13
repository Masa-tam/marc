#include "frame/lz77_profile.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace {
using namespace marc::frame;

TEST(Lz77Profile, BuildsCanonicalProfileAndWorstCaseWorkspace) {
    StreamHeader stream{};
    Lz77EncoderWorkspaceRequirements workspace{};
    Lz77ProfileConfig config{};
    config.original_size = 7;
    config.frame_size = 4;
    const auto error = make_lz77_profile(config, {}, stream, workspace);
    EXPECT_EQ(error, Lz77ProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lz77);
    EXPECT_EQ(stream.dictionary_parameters_size, 16U);
    EXPECT_EQ(workspace.frame_input_bytes, 4U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 4U * 16U);
}

TEST(Lz77Profile, EmptyEncoderNeedsNoFramePayloadWorkspace) {
    StreamHeader stream{};
    Lz77EncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(make_lz77_profile({}, {}, stream, workspace),
              Lz77ProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size);
}

TEST(Lz77Profile, RejectsParameterAndAggregateLimits) {
    StreamHeader stream{};
    Lz77EncoderWorkspaceRequirements workspace{};
    Lz77ProfileConfig config{};
    config.original_size = 4;
    config.frame_size = 4;
    config.parameters.min_match_length = 2;
    EXPECT_EQ(make_lz77_profile(config, {}, stream, workspace),
              Lz77ProfileError::invalid_configuration);

    config.parameters = {};
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 123;
    EXPECT_EQ(make_lz77_profile(config, limits, stream, workspace),
              Lz77ProfileError::limit_exceeded);
}

TEST(Lz77Profile, DecoderWorkspaceComesOnlyFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 1024;
    limits.max_compressed_payload_size = 2048;
    limits.max_internal_buffered_bytes = 4096;
    limits.max_block_size = 4096;
    Lz77DecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lz77_decoder_workspace(limits, workspace),
              Lz77ProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, frame_header_size + 1024U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
}

TEST(Lz77Profile, RejectsInvalidLimitsAndHostOverflow) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    Lz77DecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(calculate_lz77_decoder_workspace(limits, workspace),
              Lz77ProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_total_output_size =
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            + 1;
        limits.max_frame_size = limits.max_total_output_size;
        EXPECT_EQ(calculate_lz77_decoder_workspace(limits, workspace),
                  Lz77ProfileError::arithmetic_overflow);
    }
}

} // namespace

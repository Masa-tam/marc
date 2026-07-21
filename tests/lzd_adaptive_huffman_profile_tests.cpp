#include "frame/lzd_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdAdaptiveHuffmanProfileError;
using marc::frame::LzdAdaptiveHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage,
    const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzdAdaptiveHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzdAdaptiveHuffmanProfileConfig config{17, 10, {}};
    ASSERT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzd);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 40U * 33U);
    EXPECT_EQ(workspace.encoder_entry_count, 5U);
    EXPECT_EQ(workspace.views_bytes,
              5U * sizeof(marc::dictionary::internal::LzdEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzdEncoderEntry));
}

TEST(LzdAdaptiveHuffmanProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzdAdaptiveHuffmanProfileConfig config{7, 16, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 32U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    config = {1, 16, {}};
    ASSERT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 8U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    marc::frame::LzdAdaptiveHuffmanEncoderViews empty_views{};
    EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_encoder_views(
                  workspace, {}, empty_views),
              LzdAdaptiveHuffmanWorkspaceError::none);
    EXPECT_TRUE(empty_views.entries.empty());

    ASSERT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzdAdaptiveHuffmanProfile, EnforcesTokenPayloadAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    EXPECT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 263;
    EXPECT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 344;
    limits.max_block_size = 344;
    EXPECT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdAdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(LzdAdaptiveHuffmanProfile, CalculatesCoupledDecoderLayout) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.expansion_entry_count, 11U);
    EXPECT_EQ(workspace.expansion_offset % alignof(std::uint32_t), 0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max(alignof(marc::dictionary::internal::LzdPhraseEntry),
                       alignof(std::uint32_t)));
    EXPECT_EQ(workspace.views_bytes,
              workspace.expansion_offset
                  + workspace.expansion_entry_count * sizeof(std::uint32_t));

    limits.max_frame_size = 10;
    limits.max_dictionary_entries = 100;
    ASSERT_EQ(
        marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzdAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.phrase_entry_count, 5U);
    EXPECT_EQ(workspace.expansion_entry_count, 6U);
}

TEST(LzdAdaptiveHuffmanProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzd_adaptive_huffman_profile(
                  {4, 4, {}}, {}, stream, workspace),
              LzdAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdAdaptiveHuffmanEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_adaptive_huffman_encoder_views(
                  workspace, storage, views),
              LzdAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdAdaptiveHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_encoder_views(
                      workspace, misaligned, views),
                  LzdAdaptiveHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzdAdaptiveHuffmanProfile, PartitionsPhraseAndExpansionViews) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzdAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdAdaptiveHuffmanDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_adaptive_huffman_decoder_views(
                  workspace, storage, views),
              LzdAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(views.phrases.size(), workspace.phrase_entry_count);
    EXPECT_EQ(views.expansion.size(), workspace.expansion_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.phrases.data()),
              storage.data());
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.expansion.data()),
              storage.data() + workspace.expansion_offset);
    EXPECT_LE(reinterpret_cast<std::byte*>(views.phrases.data())
                  + views.phrases.size_bytes(),
              reinterpret_cast<std::byte*>(views.expansion.data()));

    auto invalid = workspace;
    ++invalid.expansion_offset;
    EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_decoder_views(
                  invalid, storage, views),
              LzdAdaptiveHuffmanWorkspaceError::invalid_requirements);
    EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_decoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdAdaptiveHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_adaptive_huffman_decoder_views(
                      workspace, misaligned, views),
                  LzdAdaptiveHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzdAdaptiveHuffmanProfile, MapsErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzd_adaptive_huffman_profile_error_code(
                  LzdAdaptiveHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzd_adaptive_huffman_profile_error_code(
                  LzdAdaptiveHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzd_adaptive_huffman_profile_error_code(
                  LzdAdaptiveHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzd_adaptive_huffman_profile_error_code(
                  LzdAdaptiveHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzdAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(
        marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzdAdaptiveHuffmanProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_internal_buffered_bytes =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(
            marc::frame::calculate_lzd_adaptive_huffman_decoder_workspace(
                limits, workspace),
            LzdAdaptiveHuffmanProfileError::arithmetic_overflow);
    }
}

} // namespace

#include "frame/lzmw_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::LzmwAdaptiveHuffmanProfileError;
using marc::frame::LzmwAdaptiveHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage,
    const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzmwAdaptiveHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzmwAdaptiveHuffmanProfileConfig config{17, 10, {}};
    ASSERT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzmw);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 40U * 33U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzmwEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzmwEncoderEntry));
}

TEST(LzmwAdaptiveHuffmanProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzmwAdaptiveHuffmanProfileConfig config{7, 16, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 28U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    config = {1, 16, {}};
    ASSERT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 4U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    marc::frame::LzmwAdaptiveHuffmanEncoderViews empty_views{};
    EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                  workspace, {}, empty_views),
              LzmwAdaptiveHuffmanWorkspaceError::none);
    EXPECT_TRUE(empty_views.entries.empty());
    auto invalid_empty = workspace;
    invalid_empty.views_alignment = 2;
    EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                  invalid_empty, {}, empty_views),
              LzmwAdaptiveHuffmanWorkspaceError::invalid_requirements);

    ASSERT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzmwAdaptiveHuffmanProfile, EnforcesReferencePayloadAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 3;
    EXPECT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 131;
    EXPECT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 208;
    limits.max_block_size = 208;
    EXPECT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(LzmwAdaptiveHuffmanProfile, CalculatesCoupledDecoderLayout) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzmwAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.expansion_entry_count, 11U);
    EXPECT_EQ(workspace.expansion_offset % alignof(std::uint32_t), 0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max(alignof(marc::dictionary::internal::LzmwPhraseEntry),
                       alignof(std::uint32_t)));
    EXPECT_EQ(workspace.views_bytes,
              workspace.expansion_offset
                  + workspace.expansion_entry_count * sizeof(std::uint32_t));

    limits.max_frame_size = 10;
    limits.max_dictionary_entries = 100;
    ASSERT_EQ(
        marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzmwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.phrase_entry_count, 31U);
    EXPECT_EQ(workspace.expansion_entry_count, 32U);
}

TEST(LzmwAdaptiveHuffmanProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzmw_adaptive_huffman_profile(
                  {4, 4, {}}, {}, stream, workspace),
              LzmwAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzmwAdaptiveHuffmanEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                  workspace, storage, views),
              LzmwAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzmwAdaptiveHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                      workspace, misaligned, views),
                  LzmwAdaptiveHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzmwAdaptiveHuffmanProfile, PartitionsPhraseAndExpansionViews) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzmwAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzmwAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzmwAdaptiveHuffmanDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzmw_adaptive_huffman_decoder_views(
                  workspace, storage, views),
              LzmwAdaptiveHuffmanWorkspaceError::none);
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
    EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_decoder_views(
                  invalid, storage, views),
              LzmwAdaptiveHuffmanWorkspaceError::invalid_requirements);
    EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_decoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzmwAdaptiveHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzmw_adaptive_huffman_decoder_views(
                      workspace, misaligned, views),
                  LzmwAdaptiveHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzmwAdaptiveHuffmanProfile, MapsErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzmw_adaptive_huffman_profile_error_code(
                  LzmwAdaptiveHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzmw_adaptive_huffman_profile_error_code(
                  LzmwAdaptiveHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzmw_adaptive_huffman_profile_error_code(
                  LzmwAdaptiveHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzmw_adaptive_huffman_profile_error_code(
                  LzmwAdaptiveHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzmwAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(
        marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzmwAdaptiveHuffmanProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_internal_buffered_bytes =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(
            marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
                limits, workspace),
            LzmwAdaptiveHuffmanProfileError::arithmetic_overflow);
    }
}

} // namespace

#include "frame/lzmw_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::LzmwBlockedHuffmanProfileError;
using marc::frame::LzmwBlockedHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage, const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzmwBlockedHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzmwBlockedHuffmanProfileConfig config{
        17, 10, 16, {}};
    ASSERT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzmw);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 40U + 3U * 16U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzmwEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzmwEncoderEntry));
}

TEST(LzmwBlockedHuffmanProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzmwBlockedHuffmanProfileConfig config{7, 16, 64, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 28U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    config = {1, 16, 64, {}};
    ASSERT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 4U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    marc::frame::LzmwBlockedHuffmanEncoderViews empty_views{};
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                  workspace, {}, empty_views),
              LzmwBlockedHuffmanWorkspaceError::none);
    EXPECT_TRUE(empty_views.entries.empty());
    auto invalid_empty = workspace;
    invalid_empty.views_alignment = 2;
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                  invalid_empty, {}, empty_views),
              LzmwBlockedHuffmanWorkspaceError::invalid_requirements);

    ASSERT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzmwBlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  {3, 3, 8, {}}, limits, stream, workspace),
              LzmwBlockedHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 8;
    const std::uint64_t required = 1 + 4 + (56 + 16 + 4);
    limits.max_internal_buffered_bytes = required - 1;
    EXPECT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  {1, 1, 8, {}}, limits, stream, workspace),
              LzmwBlockedHuffmanProfileError::limit_exceeded);
}

TEST(LzmwBlockedHuffmanProfile, CalculatesCoupledDecoderLayout) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::LzmwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.block_view_count, 4U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.expansion_entry_count, 11U);
    EXPECT_EQ(workspace.phrase_offset
                  % alignof(marc::dictionary::internal::LzmwPhraseEntry),
              0U);
    EXPECT_EQ(workspace.expansion_offset % alignof(std::uint32_t), 0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max({alignof(marc::entropy::internal::
                                    BlockedHuffmanBlockView),
                        alignof(marc::dictionary::internal::LzmwPhraseEntry),
                        alignof(std::uint32_t)}));
    EXPECT_EQ(workspace.views_bytes,
              workspace.expansion_offset
                  + workspace.expansion_entry_count * sizeof(std::uint32_t));

    limits.max_frame_size = 10;
    limits.max_dictionary_entries = 100;
    ASSERT_EQ(marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzmwBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.phrase_entry_count, 31U);
    EXPECT_EQ(workspace.expansion_entry_count, 32U);
}

TEST(LzmwBlockedHuffmanProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzmw_blocked_huffman_profile(
                  {4, 4, 32, {}}, {}, stream, workspace),
              LzmwBlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzmwBlockedHuffmanEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                  workspace, storage, views),
              LzmwBlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzmwBlockedHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                      workspace, misaligned, views),
                  LzmwBlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzmwBlockedHuffmanProfile, PartitionsThreeDecoderViews) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::LzmwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzmwBlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzmwBlockedHuffmanDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                  workspace, storage, views),
              LzmwBlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.blocks.size(), workspace.block_view_count);
    EXPECT_EQ(views.phrases.size(), workspace.phrase_entry_count);
    EXPECT_EQ(views.expansion.size(), workspace.expansion_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.blocks.data()),
              storage.data());
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.phrases.data()),
              storage.data() + workspace.phrase_offset);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.expansion.data()),
              storage.data() + workspace.expansion_offset);
    EXPECT_LE(reinterpret_cast<std::byte*>(views.blocks.data())
                  + views.blocks.size_bytes(),
              reinterpret_cast<std::byte*>(views.phrases.data()));
    EXPECT_LE(reinterpret_cast<std::byte*>(views.phrases.data())
                  + views.phrases.size_bytes(),
              reinterpret_cast<std::byte*>(views.expansion.data()));

    auto invalid = workspace;
    ++invalid.expansion_offset;
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                  invalid, storage, views),
              LzmwBlockedHuffmanWorkspaceError::invalid_requirements);
    invalid = workspace;
    ++invalid.phrase_offset;
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                  invalid, storage, views),
              LzmwBlockedHuffmanWorkspaceError::invalid_requirements);
    EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzmwBlockedHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                      workspace, misaligned, views),
                  LzmwBlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzmwBlockedHuffmanProfile, MapsErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzmw_blocked_huffman_profile_error_code(
                  LzmwBlockedHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzmw_blocked_huffman_profile_error_code(
                  LzmwBlockedHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzmw_blocked_huffman_profile_error_code(
                  LzmwBlockedHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzmw_blocked_huffman_profile_error_code(
                  LzmwBlockedHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzmwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzmwBlockedHuffmanProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_internal_buffered_bytes =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                      limits, workspace),
                  LzmwBlockedHuffmanProfileError::arithmetic_overflow);
    }
}

} // namespace

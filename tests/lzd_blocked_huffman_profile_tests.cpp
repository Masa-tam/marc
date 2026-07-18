#include "frame/lzd_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdBlockedHuffmanProfileError;
using marc::frame::LzdBlockedHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage, const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzdBlockedHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzdBlockedHuffmanProfileConfig config{
        17, 10, 16, {}};
    ASSERT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzd);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 40U + 3U * 16U);
    EXPECT_EQ(workspace.encoder_entry_count, 5U);
    EXPECT_EQ(workspace.views_bytes,
              5U * sizeof(marc::dictionary::internal::LzdEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzdEncoderEntry));
}

TEST(LzdBlockedHuffmanProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::LzdBlockedHuffmanProfileConfig config{7, 16, 64, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 32U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    config = {1, 16, 64, {}};
    ASSERT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  config, {}, stream, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 8U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    marc::frame::LzdBlockedHuffmanEncoderViews empty_views{};
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_encoder_views(
                  workspace, {}, empty_views),
              LzdBlockedHuffmanWorkspaceError::none);
    EXPECT_TRUE(empty_views.entries.empty());
    auto invalid_empty = workspace;
    invalid_empty.views_alignment = 2;
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_encoder_views(
                  invalid_empty, {}, empty_views),
              LzdBlockedHuffmanWorkspaceError::invalid_requirements);

    ASSERT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzdBlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  {3, 3, 8, {}}, limits, stream, workspace),
              LzdBlockedHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 8;
    const std::uint64_t required = 1 + 8 + (56 + 16 + 8);
    limits.max_internal_buffered_bytes = required - 1;
    EXPECT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  {1, 1, 8, {}}, limits, stream, workspace),
              LzdBlockedHuffmanProfileError::limit_exceeded);
}

TEST(LzdBlockedHuffmanProfile, CalculatesCoupledDecoderLayout) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.block_view_count, 4U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.expansion_entry_count, 11U);
    EXPECT_EQ(workspace.phrase_offset
                  % alignof(marc::dictionary::internal::LzdPhraseEntry),
              0U);
    EXPECT_EQ(workspace.expansion_offset % alignof(std::uint32_t), 0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max({alignof(marc::entropy::internal::
                                    BlockedHuffmanBlockView),
                        alignof(marc::dictionary::internal::LzdPhraseEntry),
                        alignof(std::uint32_t)}));
    EXPECT_EQ(workspace.views_bytes,
              workspace.expansion_offset
                  + workspace.expansion_entry_count * sizeof(std::uint32_t));

    limits.max_frame_size = 10;
    limits.max_dictionary_entries = 100;
    ASSERT_EQ(marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzdBlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.phrase_entry_count, 5U);
    EXPECT_EQ(workspace.expansion_entry_count, 6U);
}

TEST(LzdBlockedHuffmanProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdBlockedHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzd_blocked_huffman_profile(
                  {4, 4, 32, {}}, {}, stream, workspace),
              LzdBlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdBlockedHuffmanEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_blocked_huffman_encoder_views(
                  workspace, storage, views),
              LzdBlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdBlockedHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_encoder_views(
                      workspace, misaligned, views),
                  LzdBlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzdBlockedHuffmanProfile, PartitionsThreeDecoderViews) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzdBlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdBlockedHuffmanDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_blocked_huffman_decoder_views(
                  workspace, storage, views),
              LzdBlockedHuffmanWorkspaceError::none);
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
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_decoder_views(
                  invalid, storage, views),
              LzdBlockedHuffmanWorkspaceError::invalid_requirements);
    invalid = workspace;
    ++invalid.phrase_offset;
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_decoder_views(
                  invalid, storage, views),
              LzdBlockedHuffmanWorkspaceError::invalid_requirements);
    EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_decoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdBlockedHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_blocked_huffman_decoder_views(
                      workspace, misaligned, views),
                  LzdBlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzdBlockedHuffmanProfile, MapsErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzd_blocked_huffman_profile_error_code(
                  LzdBlockedHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzd_blocked_huffman_profile_error_code(
                  LzdBlockedHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzd_blocked_huffman_profile_error_code(
                  LzdBlockedHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzd_blocked_huffman_profile_error_code(
                  LzdBlockedHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzdBlockedHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                  limits, workspace),
              LzdBlockedHuffmanProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_internal_buffered_bytes =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(marc::frame::calculate_lzd_blocked_huffman_decoder_workspace(
                      limits, workspace),
                  LzdBlockedHuffmanProfileError::arithmetic_overflow);
    }
}

} // namespace

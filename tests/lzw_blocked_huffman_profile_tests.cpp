#include "frame/lzw_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzwBlockedHuffmanProfileError;
using marc::frame::LzwBlockedHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte>
aligned_storage(std::vector<std::byte> &storage, const std::size_t bytes,
                const std::size_t alignment) {
  const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
  const auto remainder = address % alignment;
  const auto offset = remainder == 0 ? 0 : alignment - remainder;
  return {storage.data() + offset, bytes};
}

TEST(LzwBlockedHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
  marc::frame::StreamHeader stream{};
  marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
  const marc::frame::LzwBlockedHuffmanProfileConfig config{17, 10, 16, {}};
  ASSERT_EQ(marc::frame::make_lzw_blocked_huffman_profile(config, {}, stream,
                                                          workspace),
            LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(stream.dictionary_algorithm, marc::frame::DictionaryAlgorithm::lzw);
  EXPECT_EQ(stream.entropy_algorithm,
            marc::frame::EntropyAlgorithm::blocked_huffman);
  EXPECT_EQ(workspace.frame_input_bytes, 10U);
  EXPECT_EQ(workspace.dictionary_staging_bytes, 20U);
  EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 20U + 2U * 16U);
  EXPECT_EQ(workspace.encoder_entry_count, 9U);
  EXPECT_EQ(workspace.views_bytes,
            9U * sizeof(marc::dictionary::internal::LzwEncoderEntry));
  EXPECT_EQ(workspace.views_alignment,
            alignof(marc::dictionary::internal::LzwEncoderEntry));
}

TEST(LzwBlockedHuffmanProfile, HonorsShortFrameAndEmptyStream) {
  marc::frame::StreamHeader stream{};
  marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
  const marc::frame::LzwBlockedHuffmanProfileConfig config{7, 16, 64, {}};
  ASSERT_EQ(marc::frame::make_lzw_blocked_huffman_profile(config, {}, stream,
                                                          workspace),
            LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(workspace.frame_input_bytes, 7U);
  EXPECT_EQ(workspace.dictionary_staging_bytes, 14U);
  EXPECT_EQ(workspace.encoder_entry_count, 6U);

  ASSERT_EQ(
      marc::frame::make_lzw_blocked_huffman_profile({}, {}, stream, workspace),
      LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(workspace.frame_input_bytes, 0U);
  EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
  EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
  EXPECT_EQ(workspace.views_bytes, 0U);
  EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzwBlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
  marc::frame::StreamHeader stream{};
  marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
  marc::core::DecoderLimits limits{};
  limits.max_blocks_per_frame = 1;
  EXPECT_EQ(marc::frame::make_lzw_blocked_huffman_profile({2, 2, 2, {}}, limits,
                                                          stream, workspace),
            LzwBlockedHuffmanProfileError::limit_exceeded);

  limits = {};
  limits.max_block_size = 2;
  limits.max_internal_buffered_bytes = 1 + 2 + (56 + 16 + 2) - 1;
  EXPECT_EQ(marc::frame::make_lzw_blocked_huffman_profile({1, 1, 2, {}}, limits,
                                                          stream, workspace),
            LzwBlockedHuffmanProfileError::limit_exceeded);
}

TEST(LzwBlockedHuffmanProfile, CalculatesDecoderLayoutFromLocalLimits) {
  marc::core::DecoderLimits limits{};
  limits.max_frame_size = 64;
  limits.max_block_size = 128;
  limits.max_dictionary_serialized_size = 128;
  limits.max_internal_buffered_bytes = 1024;
  limits.max_blocks_per_frame = 4;
  limits.max_dictionary_entries = 300;
  marc::frame::LzwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
  ASSERT_EQ(marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, workspace),
            LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
  EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
  EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
  EXPECT_EQ(workspace.block_view_count, 4U);
  EXPECT_EQ(workspace.phrase_entry_count, 112U);
  EXPECT_EQ(workspace.phrase_offset %
                alignof(marc::dictionary::internal::LzwPhraseEntry),
            0U);
  EXPECT_EQ(workspace.views_alignment,
            std::max(alignof(marc::entropy::internal::BlockedHuffmanBlockView),
                     alignof(marc::dictionary::internal::LzwPhraseEntry)));
  EXPECT_EQ(workspace.views_bytes,
            workspace.phrase_offset +
                workspace.phrase_entry_count *
                    sizeof(marc::dictionary::internal::LzwPhraseEntry));
}

TEST(LzwBlockedHuffmanProfile, EmptyEncoderViewsUseNeutralAlignment) {
  marc::core::DecoderLimits limits{};
  marc::frame::StreamHeader stream{};
  marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements workspace{};

  ASSERT_EQ(marc::frame::make_lzw_blocked_huffman_profile(
                {0, 64, 64, {9, 0, 0}}, limits, stream, workspace),
            LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(workspace.views_bytes, 0U);
  EXPECT_EQ(workspace.views_alignment, 1U);

  ASSERT_EQ(marc::frame::make_lzw_blocked_huffman_profile(
                {1, 64, 64, {9, 0, 0}}, limits, stream, workspace),
            LzwBlockedHuffmanProfileError::none);
  EXPECT_EQ(workspace.encoder_entry_count, 0U);
  EXPECT_EQ(workspace.views_bytes, 0U);
  EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzwBlockedHuffmanProfile, PartitionsEncoderOpaqueStorage) {
  marc::frame::StreamHeader stream{};
  marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements workspace{};
  ASSERT_EQ(marc::frame::make_lzw_blocked_huffman_profile({4, 4, 32, {}}, {},
                                                          stream, workspace),
            LzwBlockedHuffmanProfileError::none);
  std::vector<std::byte> allocation(workspace.views_bytes +
                                    workspace.views_alignment);
  auto storage = aligned_storage(allocation, workspace.views_bytes,
                                 workspace.views_alignment);
  marc::frame::LzwBlockedHuffmanEncoderViews views{};
  ASSERT_EQ(marc::frame::partition_lzw_blocked_huffman_encoder_views(
                workspace, storage, views),
            LzwBlockedHuffmanWorkspaceError::none);
  EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
  EXPECT_EQ(reinterpret_cast<std::byte *>(views.entries.data()),
            storage.data());
  EXPECT_EQ(marc::frame::partition_lzw_blocked_huffman_encoder_views(
                workspace, storage.first(storage.size() - 1), views),
            LzwBlockedHuffmanWorkspaceError::too_small);
  if (workspace.views_alignment > 1) {
    auto misaligned = std::span<std::byte>{storage.data() + 1, storage.size()};
    EXPECT_EQ(marc::frame::partition_lzw_blocked_huffman_encoder_views(
                  workspace, misaligned, views),
              LzwBlockedHuffmanWorkspaceError::misaligned);
  }
}

TEST(LzwBlockedHuffmanProfile, PartitionsDecoderOpaqueStorage) {
  marc::core::DecoderLimits limits{};
  limits.max_frame_size = 64;
  limits.max_block_size = 128;
  limits.max_dictionary_serialized_size = 128;
  limits.max_internal_buffered_bytes = 1024;
  limits.max_blocks_per_frame = 4;
  limits.max_dictionary_entries = 300;
  marc::frame::LzwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
  ASSERT_EQ(marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, workspace),
            LzwBlockedHuffmanProfileError::none);
  std::vector<std::byte> allocation(workspace.views_bytes +
                                    workspace.views_alignment);
  auto storage = aligned_storage(allocation, workspace.views_bytes,
                                 workspace.views_alignment);
  marc::frame::LzwBlockedHuffmanDecoderViews views{};
  ASSERT_EQ(marc::frame::partition_lzw_blocked_huffman_decoder_views(
                workspace, storage, views),
            LzwBlockedHuffmanWorkspaceError::none);
  EXPECT_EQ(views.blocks.size(), workspace.block_view_count);
  EXPECT_EQ(views.phrases.size(), workspace.phrase_entry_count);
  EXPECT_EQ(reinterpret_cast<std::byte *>(views.blocks.data()), storage.data());
  EXPECT_EQ(reinterpret_cast<std::byte *>(views.phrases.data()),
            storage.data() + workspace.phrase_offset);
  EXPECT_LE(reinterpret_cast<std::byte *>(views.blocks.data()) +
                views.blocks.size_bytes(),
            reinterpret_cast<std::byte *>(views.phrases.data()));

  auto invalid = workspace;
  ++invalid.phrase_offset;
  EXPECT_EQ(marc::frame::partition_lzw_blocked_huffman_decoder_views(
                invalid, storage, views),
            LzwBlockedHuffmanWorkspaceError::invalid_requirements);
  if (workspace.views_alignment > 1) {
    auto misaligned = std::span<std::byte>{storage.data() + 1, storage.size()};
    EXPECT_EQ(marc::frame::partition_lzw_blocked_huffman_decoder_views(
                  workspace, misaligned, views),
              LzwBlockedHuffmanWorkspaceError::misaligned);
  }
}

TEST(LzwBlockedHuffmanProfile, MapsStableErrorsAndRejectsInvalidLimits) {
  EXPECT_EQ(marc::frame::lzw_blocked_huffman_profile_error_code(
                LzwBlockedHuffmanProfileError::none),
            marc::core::ErrorCode::none);
  EXPECT_EQ(marc::frame::lzw_blocked_huffman_profile_error_code(
                LzwBlockedHuffmanProfileError::invalid_configuration),
            marc::core::ErrorCode::invalid_argument);
  EXPECT_EQ(marc::frame::lzw_blocked_huffman_profile_error_code(
                LzwBlockedHuffmanProfileError::unsupported),
            marc::core::ErrorCode::unsupported);
  EXPECT_EQ(marc::frame::lzw_blocked_huffman_profile_error_code(
                LzwBlockedHuffmanProfileError::limit_exceeded),
            marc::core::ErrorCode::limit_exceeded);

  marc::core::DecoderLimits limits{};
  limits.max_frame_size = limits.max_total_output_size + 1;
  marc::frame::LzwBlockedHuffmanDecoderWorkspaceRequirements workspace{};
  EXPECT_EQ(marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, workspace),
            LzwBlockedHuffmanProfileError::invalid_configuration);

  limits = {};
  limits.max_dictionary_entries = 253;
  EXPECT_EQ(marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, workspace),
            LzwBlockedHuffmanProfileError::limit_exceeded);
}

} // namespace

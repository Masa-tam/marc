#include "frame/lz78_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz78BlockedHuffmanProfileError;
using marc::frame::Lz78BlockedHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage, const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(Lz78BlockedHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::Lz78BlockedHuffmanProfileConfig config{
        17, 10, 16, {}};
    ASSERT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  config, {}, stream, workspace),
              Lz78BlockedHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lz78);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::blocked_huffman);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 80U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 80U + 5U * 16U);
    EXPECT_EQ(workspace.encoder_entry_count, 10U);
    EXPECT_EQ(workspace.views_bytes,
              10U * sizeof(marc::dictionary::internal::Lz78EncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::Lz78EncoderEntry));
}

TEST(Lz78BlockedHuffmanProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::frame::Lz78BlockedHuffmanProfileConfig config{7, 16, 64, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  config, {}, stream, workspace),
              Lz78BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 56U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    ASSERT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  {}, {}, stream, workspace),
              Lz78BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(Lz78BlockedHuffmanProfile, EnforcesBlockAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_blocks_per_frame = 1;
    EXPECT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  {2, 2, 8, {}}, limits, stream, workspace),
              Lz78BlockedHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_block_size = 8;
    const std::uint64_t needed = 1 + 8 + (56 + 16 + 8)
        + sizeof(marc::dictionary::internal::Lz78EncoderEntry);
    limits.max_internal_buffered_bytes = needed - 1;
    EXPECT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  {1, 1, 8, {}}, limits, stream, workspace),
              Lz78BlockedHuffmanProfileError::limit_exceeded);
}

TEST(Lz78BlockedHuffmanProfile, CalculatesDecoderLayoutFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::Lz78BlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lz78_blocked_huffman_decoder_workspace(
                  limits, workspace),
              Lz78BlockedHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.block_view_count, 4U);
    EXPECT_EQ(workspace.phrase_entry_count, 10U);
    EXPECT_EQ(workspace.phrase_offset
                  % alignof(marc::dictionary::internal::Lz78PhraseEntry),
              0U);
    EXPECT_EQ(workspace.views_alignment,
              std::max(alignof(marc::entropy::internal::
                                   BlockedHuffmanBlockView),
                       alignof(marc::dictionary::internal::Lz78PhraseEntry)));
    EXPECT_EQ(workspace.views_bytes,
              workspace.phrase_offset
                  + workspace.phrase_entry_count
                      * sizeof(marc::dictionary::internal::Lz78PhraseEntry));
}

TEST(Lz78BlockedHuffmanProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lz78_blocked_huffman_profile(
                  {4, 4, 32, {}}, {}, stream, workspace),
              Lz78BlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::Lz78BlockedHuffmanEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lz78_blocked_huffman_encoder_views(
                  workspace, storage, views),
              Lz78BlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lz78_blocked_huffman_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              Lz78BlockedHuffmanWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        EXPECT_EQ(marc::frame::partition_lz78_blocked_huffman_encoder_views(
                      workspace, storage.subspan(1), views),
                  Lz78BlockedHuffmanWorkspaceError::too_small);
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lz78_blocked_huffman_encoder_views(
                      workspace, misaligned, views),
                  Lz78BlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(Lz78BlockedHuffmanProfile, PartitionsDecoderOpaqueStorage) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_blocks_per_frame = 4;
    limits.max_dictionary_entries = 10;
    marc::frame::Lz78BlockedHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::calculate_lz78_blocked_huffman_decoder_workspace(
                  limits, workspace),
              Lz78BlockedHuffmanProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::Lz78BlockedHuffmanDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lz78_blocked_huffman_decoder_views(
                  workspace, storage, views),
              Lz78BlockedHuffmanWorkspaceError::none);
    EXPECT_EQ(views.blocks.size(), workspace.block_view_count);
    EXPECT_EQ(views.phrases.size(), workspace.phrase_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.blocks.data()),
              storage.data());
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.phrases.data()),
              storage.data() + workspace.phrase_offset);
    EXPECT_LE(reinterpret_cast<std::byte*>(views.blocks.data())
                  + views.blocks.size_bytes(),
              reinterpret_cast<std::byte*>(views.phrases.data()));

    auto invalid = workspace;
    ++invalid.phrase_offset;
    EXPECT_EQ(marc::frame::partition_lz78_blocked_huffman_decoder_views(
                  invalid, storage, views),
              Lz78BlockedHuffmanWorkspaceError::invalid_requirements);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lz78_blocked_huffman_decoder_views(
                      workspace, misaligned, views),
                  Lz78BlockedHuffmanWorkspaceError::misaligned);
    }
}

TEST(Lz78BlockedHuffmanProfile, MapsStableErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lz78_blocked_huffman_profile_error_code(
                  Lz78BlockedHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lz78_blocked_huffman_profile_error_code(
                  Lz78BlockedHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lz78_blocked_huffman_profile_error_code(
                  Lz78BlockedHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lz78_blocked_huffman_profile_error_code(
                  Lz78BlockedHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::Lz78BlockedHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(marc::frame::calculate_lz78_blocked_huffman_decoder_workspace(
                  limits, workspace),
              Lz78BlockedHuffmanProfileError::invalid_configuration);
}

} // namespace

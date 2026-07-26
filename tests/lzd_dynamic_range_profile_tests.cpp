#include "frame/lzd_dynamic_range_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using marc::frame::LzdDynamicRangeProfileError;
using marc::frame::LzdDynamicRangeWorkspaceError;

[[nodiscard]] std::span<std::byte> aligned_storage(
    std::vector<std::byte>& storage,
    const std::size_t bytes,
    const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzdDynamicRangeProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzdDynamicRangeProfileConfig config{17, 10, {}};
    ASSERT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  config, {}, stream, workspace),
              LzdDynamicRangeProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzd);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::dynamic_range);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 40U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 85U);
    EXPECT_EQ(workspace.encoder_entry_count, 5U);
    EXPECT_EQ(workspace.views_bytes,
              5U * sizeof(marc::dictionary::internal::LzdEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzdEncoderEntry));
}

TEST(LzdDynamicRangeProfile, HonorsFreezeShortFrameAndEmptyStream) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements workspace{};
    marc::frame::LzdDynamicRangeProfileConfig config{7, 16, {}};
    config.parameters.maximum_entries = 2;
    ASSERT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  config, {}, stream, workspace),
              LzdDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 32U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 69U);
    EXPECT_EQ(workspace.encoder_entry_count, 2U);

    config = {1, 16, {}};
    ASSERT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  config, {}, stream, workspace),
              LzdDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 8U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
    marc::frame::LzdDynamicRangeEncoderViews empty_views{};
    EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_encoder_views(
                  workspace, {}, empty_views),
              LzdDynamicRangeWorkspaceError::none);
    EXPECT_TRUE(empty_views.entries.empty());

    ASSERT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  {}, {}, stream, workspace),
              LzdDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzdDynamicRangeProfile, EnforcesTokenPayloadAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 7;
    EXPECT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdDynamicRangeProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 20;
    EXPECT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdDynamicRangeProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 101;
    limits.max_block_size = 101;
    EXPECT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzdDynamicRangeProfileError::limit_exceeded);
}

TEST(LzdDynamicRangeProfile, CalculatesCoupledDecoderLayout) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdDynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
            limits, workspace),
        LzdDynamicRangeProfileError::none);
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
        marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
            limits, workspace),
        LzdDynamicRangeProfileError::none);
    EXPECT_EQ(workspace.phrase_entry_count, 5U);
    EXPECT_EQ(workspace.expansion_entry_count, 6U);
}

TEST(LzdDynamicRangeProfile, PartitionsEncoderOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzdDynamicRangeEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzd_dynamic_range_profile(
                  {4, 4, {}}, {}, stream, workspace),
              LzdDynamicRangeProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdDynamicRangeEncoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_dynamic_range_encoder_views(
                  workspace, storage, views),
              LzdDynamicRangeWorkspaceError::none);
    EXPECT_EQ(views.entries.size(), workspace.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(views.entries.data()),
              storage.data());
    EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_encoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdDynamicRangeWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_encoder_views(
                      workspace, misaligned, views),
                  LzdDynamicRangeWorkspaceError::misaligned);
    }
}

TEST(LzdDynamicRangeProfile, PartitionsPhraseAndExpansionViews) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 128;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_dictionary_entries = 10;
    marc::frame::LzdDynamicRangeDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
            limits, workspace),
        LzdDynamicRangeProfileError::none);
    std::vector<std::byte> allocation(
        workspace.views_bytes + workspace.views_alignment);
    auto storage = aligned_storage(
        allocation, workspace.views_bytes, workspace.views_alignment);
    marc::frame::LzdDynamicRangeDecoderViews views{};
    ASSERT_EQ(marc::frame::partition_lzd_dynamic_range_decoder_views(
                  workspace, storage, views),
              LzdDynamicRangeWorkspaceError::none);
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
    EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_decoder_views(
                  invalid, storage, views),
              LzdDynamicRangeWorkspaceError::invalid_requirements);
    EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_decoder_views(
                  workspace, storage.first(storage.size() - 1), views),
              LzdDynamicRangeWorkspaceError::too_small);
    if (workspace.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{storage.data() + 1,
                                               storage.size()};
        EXPECT_EQ(marc::frame::partition_lzd_dynamic_range_decoder_views(
                      workspace, misaligned, views),
                  LzdDynamicRangeWorkspaceError::misaligned);
    }
}

TEST(LzdDynamicRangeProfile, MapsErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzd_dynamic_range_profile_error_code(
                  LzdDynamicRangeProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzd_dynamic_range_profile_error_code(
                  LzdDynamicRangeProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzd_dynamic_range_profile_error_code(
                  LzdDynamicRangeProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzd_dynamic_range_profile_error_code(
                  LzdDynamicRangeProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzdDynamicRangeDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(
        marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
            limits, workspace),
        LzdDynamicRangeProfileError::invalid_configuration);

    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        limits = {};
        limits.max_internal_buffered_bytes =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max());
        EXPECT_EQ(
            marc::frame::calculate_lzd_dynamic_range_decoder_workspace(
                limits, workspace),
            LzdDynamicRangeProfileError::arithmetic_overflow);
    }
}

} // namespace

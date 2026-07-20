#include "frame/lzw_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzwAdaptiveHuffmanProfileError;
using marc::frame::LzwAdaptiveHuffmanWorkspaceError;

[[nodiscard]] std::span<std::byte>
aligned_storage(std::vector<std::byte>& storage,
                const std::size_t bytes,
                const std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto remainder = address % alignment;
    const auto offset = remainder == 0 ? 0 : alignment - remainder;
    return {storage.data() + offset, bytes};
}

TEST(LzwAdaptiveHuffmanProfile, BuildsCanonicalWorstCaseEncoderWorkspace) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const marc::frame::LzwAdaptiveHuffmanProfileConfig config{17, 10, {}};
    ASSERT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              LzwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::lzw);
    EXPECT_EQ(stream.entropy_algorithm,
              marc::frame::EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.frame_size, 10U);
    EXPECT_EQ(workspace.frame_input_bytes, 10U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 20U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 16U + 20U * 33U);
    EXPECT_EQ(workspace.encoder_entry_count, 9U);
    EXPECT_EQ(workspace.views_bytes,
              9U * sizeof(marc::dictionary::internal::LzwEncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzwEncoderEntry));
}

TEST(LzwAdaptiveHuffmanProfile, HonorsShortFrameAndCanonicalEmptyLayout) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {7, 16, {}}, {}, stream, workspace),
              LzwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 7U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 14U);
    EXPECT_EQ(workspace.encoder_entry_count, 6U);

    ASSERT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              LzwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 0U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 0U);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(LzwAdaptiveHuffmanProfile, EnforcesPackedPayloadAndAggregateLimits) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = 1;
    EXPECT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 65;
    EXPECT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwAdaptiveHuffmanProfileError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 140;
    limits.max_block_size = 140;
    EXPECT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {1, 1, {}}, limits, stream, workspace),
              LzwAdaptiveHuffmanProfileError::limit_exceeded);
}

TEST(LzwAdaptiveHuffmanProfile, CalculatesDecoderLayoutFromLocalLimits) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_block_size = 128;
    limits.max_dictionary_entries = 300;
    marc::frame::LzwAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(
        marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 1024U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 128U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 64U);
    EXPECT_EQ(workspace.phrase_entry_count, 112U);
    EXPECT_EQ(workspace.views_bytes,
              112U * sizeof(marc::dictionary::internal::LzwPhraseEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::LzwPhraseEntry));
}

TEST(LzwAdaptiveHuffmanProfile, EmptyEncoderViewsUseNeutralAlignment) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {1, 64, {9, 0, 0}}, {}, stream, workspace),
              LzwAdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.encoder_entry_count, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);

    marc::frame::LzwAdaptiveHuffmanEncoderViews views{};
    EXPECT_EQ(marc::frame::partition_lzw_adaptive_huffman_encoder_views(
                  workspace, {}, views),
              LzwAdaptiveHuffmanWorkspaceError::none);
    EXPECT_TRUE(views.entries.empty());
}

TEST(LzwAdaptiveHuffmanProfile, PartitionsTypedOpaqueStorage) {
    marc::frame::StreamHeader stream{};
    marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements encoder{};
    ASSERT_EQ(marc::frame::make_lzw_adaptive_huffman_profile(
                  {4, 4, {}}, {}, stream, encoder),
              LzwAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> encoder_allocation(
        encoder.views_bytes + encoder.views_alignment);
    auto encoder_storage = aligned_storage(
        encoder_allocation, encoder.views_bytes, encoder.views_alignment);
    marc::frame::LzwAdaptiveHuffmanEncoderViews encoder_views{};
    ASSERT_EQ(marc::frame::partition_lzw_adaptive_huffman_encoder_views(
                  encoder, encoder_storage, encoder_views),
              LzwAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), encoder.encoder_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(encoder_views.entries.data()),
              encoder_storage.data());
    EXPECT_EQ(marc::frame::partition_lzw_adaptive_huffman_encoder_views(
                  encoder, encoder_storage.first(encoder_storage.size() - 1),
                  encoder_views),
              LzwAdaptiveHuffmanWorkspaceError::too_small);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 64;
    limits.max_dictionary_serialized_size = 128;
    limits.max_internal_buffered_bytes = 1024;
    limits.max_block_size = 128;
    limits.max_dictionary_entries = 300;
    marc::frame::LzwAdaptiveHuffmanDecoderWorkspaceRequirements decoder{};
    ASSERT_EQ(
        marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
            limits, decoder),
        LzwAdaptiveHuffmanProfileError::none);
    std::vector<std::byte> decoder_allocation(
        decoder.views_bytes + decoder.views_alignment);
    auto decoder_storage = aligned_storage(
        decoder_allocation, decoder.views_bytes, decoder.views_alignment);
    marc::frame::LzwAdaptiveHuffmanDecoderViews decoder_views{};
    ASSERT_EQ(marc::frame::partition_lzw_adaptive_huffman_decoder_views(
                  decoder, decoder_storage, decoder_views),
              LzwAdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(decoder_views.phrases.size(), decoder.phrase_entry_count);
    EXPECT_EQ(reinterpret_cast<std::byte*>(decoder_views.phrases.data()),
              decoder_storage.data());

    auto altered = decoder;
    ++altered.views_bytes;
    EXPECT_EQ(marc::frame::partition_lzw_adaptive_huffman_decoder_views(
                  altered, decoder_storage, decoder_views),
              LzwAdaptiveHuffmanWorkspaceError::invalid_requirements);
    if (decoder.views_alignment > 1) {
        auto misaligned = std::span<std::byte>{
            decoder_storage.data() + 1, decoder_storage.size()};
        EXPECT_EQ(marc::frame::partition_lzw_adaptive_huffman_decoder_views(
                      decoder, misaligned, decoder_views),
                  LzwAdaptiveHuffmanWorkspaceError::misaligned);
    }
}

TEST(LzwAdaptiveHuffmanProfile, MapsStableErrorsAndRejectsInvalidLimits) {
    EXPECT_EQ(marc::frame::lzw_adaptive_huffman_profile_error_code(
                  LzwAdaptiveHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(marc::frame::lzw_adaptive_huffman_profile_error_code(
                  LzwAdaptiveHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(marc::frame::lzw_adaptive_huffman_profile_error_code(
                  LzwAdaptiveHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(marc::frame::lzw_adaptive_huffman_profile_error_code(
                  LzwAdaptiveHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    marc::frame::LzwAdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    EXPECT_EQ(
        marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzwAdaptiveHuffmanProfileError::invalid_configuration);

    limits = {};
    limits.max_dictionary_entries = 253;
    EXPECT_EQ(
        marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
            limits, workspace),
        LzwAdaptiveHuffmanProfileError::limit_exceeded);
}

} // namespace

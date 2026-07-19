#include "frame/lz78_adaptive_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

TEST(Lz78AdaptiveHuffmanProfile, BuildsCanonicalDefaultAndWorstCaseWorkspace) {
    StreamHeader stream{};
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    const Lz78AdaptiveHuffmanProfileConfig config{2'500'000};
    ASSERT_EQ(make_lz78_adaptive_huffman_profile(
                  config, {}, stream, workspace),
              Lz78AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(stream.dictionary_algorithm, DictionaryAlgorithm::lz78);
    EXPECT_EQ(stream.dictionary_variant, 1U);
    EXPECT_EQ(stream.entropy_algorithm,
              EntropyAlgorithm::adaptive_huffman);
    EXPECT_EQ(stream.entropy_variant, 1U);
    EXPECT_EQ(stream.frame_size, 65'536U);
    EXPECT_EQ(stream.entropy_block_size, 0U);
    EXPECT_EQ(workspace.frame_input_bytes, 65'536U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 524'288U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 17'301'576U);
    EXPECT_EQ(workspace.encoder_entry_count, 65'536U);
    EXPECT_EQ(workspace.views_bytes,
              workspace.encoder_entry_count
                  * sizeof(marc::dictionary::internal::Lz78EncoderEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::Lz78EncoderEntry));
}

TEST(Lz78AdaptiveHuffmanProfile, UsesShortFrameAndCanonicalEmptyLayout) {
    StreamHeader stream{};
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    ASSERT_EQ(make_lz78_adaptive_huffman_profile(
                  {17}, {}, stream, workspace),
              Lz78AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 17U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 136U);
    EXPECT_EQ(workspace.frame_encoded_bytes, 4'560U);
    EXPECT_EQ(workspace.encoder_entry_count, 17U);

    workspace = {1, 1, 1, 1, 1, 8};
    ASSERT_EQ(make_lz78_adaptive_huffman_profile(
                  {}, {}, stream, workspace),
              Lz78AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_input_bytes, 0U);
    EXPECT_EQ(workspace.views_bytes, 0U);
    EXPECT_EQ(workspace.views_alignment, 1U);
}

TEST(Lz78AdaptiveHuffmanProfile, RejectsLimitsAndInvalidParameters) {
    StreamHeader stream{};
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements workspace{};
    EXPECT_EQ(make_lz78_adaptive_huffman_profile(
                  {1U << 20, 1U << 20, {}}, {}, stream, workspace),
              Lz78AdaptiveHuffmanProfileError::limit_exceeded);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 2U << 20;
    EXPECT_EQ(make_lz78_adaptive_huffman_profile(
                  {(1U << 20) + 1U, (1U << 20) + 1U, {}}, limits,
                  stream, workspace),
              Lz78AdaptiveHuffmanProfileError::limit_exceeded);
    Lz78AdaptiveHuffmanProfileConfig invalid{};
    invalid.parameters.maximum_entries = 0;
    EXPECT_EQ(make_lz78_adaptive_huffman_profile(
                  invalid, {}, stream, workspace),
              Lz78AdaptiveHuffmanProfileError::invalid_configuration);
}

TEST(Lz78AdaptiveHuffmanProfile, DerivesBoundedDecoderWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 4'096;
    limits.max_block_size = 4'096;
    limits.max_dictionary_serialized_size = 6'000;
    limits.max_internal_buffered_bytes = 8'192;
    Lz78AdaptiveHuffmanDecoderWorkspaceRequirements workspace{};
    ASSERT_EQ(calculate_lz78_adaptive_huffman_decoder_workspace(
                  limits, workspace),
              Lz78AdaptiveHuffmanProfileError::none);
    EXPECT_EQ(workspace.frame_encoded_bytes, 56U + 8'192U);
    EXPECT_EQ(workspace.dictionary_staging_bytes, 6'000U);
    EXPECT_EQ(workspace.frame_decoded_bytes, 4'096U);
    EXPECT_EQ(workspace.phrase_entry_count, 750U);
    EXPECT_EQ(workspace.views_bytes,
              750U * sizeof(marc::dictionary::internal::Lz78PhraseEntry));
    EXPECT_EQ(workspace.views_alignment,
              alignof(marc::dictionary::internal::Lz78PhraseEntry));
}

TEST(Lz78AdaptiveHuffmanProfile, PartitionsAndRejectsInvalidTypedStorage) {
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements encoder{};
    encoder.encoder_entry_count = 2;
    encoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::Lz78EncoderEntry);
    encoder.views_alignment =
        alignof(marc::dictionary::internal::Lz78EncoderEntry);
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2>
        encoder_records{};
    Lz78AdaptiveHuffmanEncoderViews encoder_views{};
    ASSERT_EQ(partition_lz78_adaptive_huffman_encoder_views(
                  encoder, std::as_writable_bytes(
                               std::span{encoder_records}), encoder_views),
              Lz78AdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(encoder_views.entries.size(), 2U);

    Lz78AdaptiveHuffmanDecoderWorkspaceRequirements decoder{};
    decoder.phrase_entry_count = 2;
    decoder.views_bytes =
        2 * sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    decoder.views_alignment =
        alignof(marc::dictionary::internal::Lz78PhraseEntry);
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        phrase_records{};
    Lz78AdaptiveHuffmanDecoderViews decoder_views{};
    ASSERT_EQ(partition_lz78_adaptive_huffman_decoder_views(
                  decoder, std::as_writable_bytes(
                               std::span{phrase_records}), decoder_views),
              Lz78AdaptiveHuffmanWorkspaceError::none);
    EXPECT_EQ(decoder_views.phrases.size(), 2U);

    auto changed = decoder;
    ++changed.phrase_entry_count;
    EXPECT_EQ(partition_lz78_adaptive_huffman_decoder_views(
                  changed, std::as_writable_bytes(
                               std::span{phrase_records}), decoder_views),
              Lz78AdaptiveHuffmanWorkspaceError::invalid_requirements);
    EXPECT_EQ(partition_lz78_adaptive_huffman_decoder_views(
                  decoder,
                  std::as_writable_bytes(std::span{phrase_records}).first(
                      decoder.views_bytes - 1), decoder_views),
              Lz78AdaptiveHuffmanWorkspaceError::too_small);
    std::vector<std::byte> misaligned(decoder.views_bytes + 1);
    EXPECT_EQ(partition_lz78_adaptive_huffman_decoder_views(
                  decoder, std::span<std::byte>{misaligned}.subspan(1),
                  decoder_views),
              Lz78AdaptiveHuffmanWorkspaceError::misaligned);
}

TEST(Lz78AdaptiveHuffmanProfile, MapsStableCoreErrors) {
    EXPECT_EQ(lz78_adaptive_huffman_profile_error_code(
                  Lz78AdaptiveHuffmanProfileError::none),
              marc::core::ErrorCode::none);
    EXPECT_EQ(lz78_adaptive_huffman_profile_error_code(
                  Lz78AdaptiveHuffmanProfileError::invalid_configuration),
              marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(lz78_adaptive_huffman_profile_error_code(
                  Lz78AdaptiveHuffmanProfileError::unsupported),
              marc::core::ErrorCode::unsupported);
    EXPECT_EQ(lz78_adaptive_huffman_profile_error_code(
                  Lz78AdaptiveHuffmanProfileError::limit_exceeded),
              marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(lz78_adaptive_huffman_profile_error_code(
                  Lz78AdaptiveHuffmanProfileError::arithmetic_overflow),
              marc::core::ErrorCode::limit_exceeded);
}

} // namespace

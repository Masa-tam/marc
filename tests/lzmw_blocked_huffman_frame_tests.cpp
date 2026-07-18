#include "frame/lzmw_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

using marc::frame::LzmwBlockedHuffmanFrameValidationError;

constexpr std::array single_literal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{16}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0}};

constexpr std::array adjacent_literals_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{16}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x42}, std::byte{0}, std::byte{0}, std::byte{0}};

marc::frame::StreamHeader stream(
    const std::uint64_t size = 1,
    const std::uint32_t entropy_block_size = 4) {
    marc::frame::StreamHeader result{};
    result.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzmw;
    result.dictionary_variant = 1;
    result.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    result.entropy_variant = 1;
    result.frame_size = static_cast<std::uint32_t>(size);
    result.entropy_block_size = entropy_block_size;
    result.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    result.original_size = size;
    return result;
}

TEST(LzmwBlockedHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    const auto result = marc::frame::validate_lzmw_blocked_huffman_frame(
        stream(), {}, {}, 0, 0, single_literal_frame, views, staging, {});
    ASSERT_EQ(result.error, LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, 76U);
    EXPECT_EQ(result.dictionary_size, 4U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging[0], std::byte{'A'});
}

TEST(LzmwBlockedHuffmanFrameValidator, BuildsAdjacentPhraseRecord) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lzmw_blocked_huffman_frame(
        stream(2, 8), {}, {}, 0, 0, adjacent_literals_frame, views, staging,
        phrases);
    ASSERT_EQ(result.error, LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.token_count, 2U);
    EXPECT_EQ(result.phrase_entries, 1U);
    EXPECT_EQ(result.dictionary_entries, 1U);
    EXPECT_EQ(result.expansion_entries, 2U);
    EXPECT_EQ(phrases[0].left_reference, 0x41U);
    EXPECT_EQ(phrases[0].right_reference, 0x42U);
    EXPECT_EQ(phrases[0].length, 2U);
}

TEST(LzmwBlockedHuffmanFrameValidator, RejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzmw_blocked_huffman_frame(
                      stream(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_literal_frame}.first(
                          size),
                      views, staging, {})
                      .error,
                  LzmwBlockedHuffmanFrameValidationError::none)
            << size;
    }
    std::array<std::byte, single_literal_frame.size() + 1> extended{};
    std::ranges::copy(single_literal_frame, extended.begin());
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, extended, views, staging, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzmwBlockedHuffmanFrameValidator, RejectsCallerWorkspaceShortages) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_literal_frame, {}, staging,
                  {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::view_output_too_small);
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_literal_frame, views, {}, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);

    std::array<std::byte, 8> pair_staging{};
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(2, 8), {}, {}, 0, 0, adjacent_literals_frame, views,
                  pair_staging, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  phrase_workspace_too_small);
}

TEST(LzmwBlockedHuffmanFrameValidator,
     SeparatesEntropyAndDictionaryFailures) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    staging.fill(std::byte{0x5a});
    auto bad_descriptor = single_literal_frame;
    bad_descriptor[56] = std::byte{3};
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_descriptor, views, staging, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto forward_reference = single_literal_frame;
    forward_reference[72] = std::byte{0};
    forward_reference[73] = std::byte{1};
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, forward_reference, views, staging,
                  {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);

    std::vector<std::byte> unaligned(single_literal_frame.begin(),
                                     single_literal_frame.end() - 1);
    unaligned[20] = std::byte{3};
    unaligned[24] = std::byte{3};
    unaligned[56] = std::byte{3};
    unaligned[60] = std::byte{3};
    std::array<std::byte, 3> short_staging{};
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, unaligned, views, short_staging, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
}

TEST(LzmwBlockedHuffmanFrameDecoder, PublishesOnlyAfterCompleteValidation) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    ASSERT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_literal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output[0], std::byte{'A'});

    output[0] = std::byte{0x5a};
    EXPECT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_literal_frame, views,
                  staging, {}, {}, output)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_literal_frame, views,
                  staging, {}, expansion, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::raw_output_too_small);

    auto bad_reference = single_literal_frame;
    bad_reference[72] = std::byte{0};
    bad_reference[73] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_reference, views, staging, {},
                  expansion, output)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(LzmwBlockedHuffmanFrameDecoder, DecodesAdjacentLiterals) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 2> output{};
    ASSERT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(2, 8), {}, {}, 0, 0, adjacent_literals_frame, views,
                  staging, phrases, expansion, output)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    constexpr std::array expected{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(output, expected);
}

TEST(LzmwBlockedHuffmanFrameDecoder, EnforcesAggregateAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 4> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 4;
    limits.max_internal_buffered_bytes = 16 + 4 + 4 + sizeof(views)
        + sizeof(expansion) + output.size() - 1;
    EXPECT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  stream(), {}, limits, 0, 0, single_literal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::workspace_limit);
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto unsupported = stream();
    unsupported.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzmw_blocked_huffman_frame(
                  unsupported, {}, {}, 0, 0, single_literal_frame, views,
                  staging, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

} // namespace

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

TEST(LzmwBlockedHuffmanFrameEncoder, PlansAndEmitsSpecifiedHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 4> staging{};
    const auto plan = marc::frame::plan_lzmw_blocked_huffman_frame(
        stream(), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(plan.error, LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.raw_size, 1U);
    EXPECT_EQ(plan.dictionary_size, 4U);
    EXPECT_EQ(plan.encoder_entries, 0U);
    EXPECT_EQ(plan.dictionary_entries, 0U);
    EXPECT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.descriptor_size, 16U);
    EXPECT_EQ(plan.payload_size, 4U);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.serialized_size, single_literal_frame.size());

    std::array<std::byte, single_literal_frame.size()> encoded{};
    ASSERT_EQ(marc::frame::encode_lzmw_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, raw, {}, staging, encoded)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(encoded, single_literal_frame);
}

TEST(LzmwBlockedHuffmanFrameEncoder,
     IsDeterministicAcrossReferenceSplitsAndRoundTrips) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'A'},
        std::byte{'B'}, std::byte{'C'}};
    auto configured_stream = stream(raw.size(), 3);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, raw.size() - 1>
        encoder_workspace{};
    std::array<std::byte, raw.size() * 4> encode_staging{};
    const auto plan = marc::frame::plan_lzmw_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
        encode_staging);
    ASSERT_EQ(plan.error, LzmwBlockedHuffmanFrameValidationError::none);
    ASSERT_GT(plan.block_count, 1U);

    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, first)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, second)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
        plan.block_count);
    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::vector<marc::dictionary::internal::LzmwPhraseEntry> phrases(
        plan.dictionary_entries);
    std::vector<std::uint32_t> expansion(plan.dictionary_entries + 1U);
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, first, views,
                  decode_staging, phrases, expansion, decoded)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzmwBlockedHuffmanFrameEncoder, UsesCanonicalHuffmanWhenSmaller) {
    std::array<std::byte, 1024> raw{};
    std::uint32_t state = 0x9e3779b9U;
    for (auto& value : raw) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        value = static_cast<std::byte>(state & 0xffU);
    }
    auto configured_stream = stream(raw.size(), raw.size() * 4);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, raw.size() - 1>
        encoder_workspace{};
    std::array<std::byte, raw.size() * 4> encode_staging{};
    const auto plan = marc::frame::plan_lzmw_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
        encode_staging);
    ASSERT_EQ(plan.error, LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.descriptor_size, 272U);
    EXPECT_LT(plan.payload_size, plan.dictionary_size);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, encoded)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::vector<marc::dictionary::internal::LzmwPhraseEntry> phrases(
        plan.dictionary_entries);
    std::vector<std::uint32_t> expansion(plan.dictionary_entries + 1U);
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, encoded, views,
                  decode_staging, phrases, expansion, decoded)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(views[0].descriptor.flags, 0U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzmwBlockedHuffmanFrameEncoder,
     RejectsWorkspaceAndOutputCapacityAtomically) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    auto configured_stream = stream(raw.size(), 8);
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, {}, staging)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> workspace{};
    EXPECT_EQ(marc::frame::plan_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, workspace, {})
                  .error,
              LzmwBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);

    std::array<std::byte, adjacent_literals_frame.size() - 1> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lzmw_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, workspace, staging,
        short_output);
    EXPECT_EQ(result.error, LzmwBlockedHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, adjacent_literals_frame.size());
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzmwBlockedHuffmanFrameEncoder,
     EnforcesAggregateWorkspaceAndFrameExtent) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    auto configured_stream = stream(raw.size(), 8);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> workspace{};
    std::array<std::byte, 8> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes =
        sizeof(workspace) + staging.size() - 1;
    EXPECT_EQ(marc::frame::plan_lzmw_blocked_huffman_frame(
                  configured_stream, {}, limits, 0, 0, raw, workspace,
                  staging)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::workspace_limit);

    EXPECT_EQ(marc::frame::plan_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0,
                  std::span<const std::byte>{}, workspace, staging)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::input_size_mismatch);
    constexpr std::array too_long{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 2>
        larger_workspace{};
    std::array<std::byte, 12> larger_staging{};
    EXPECT_EQ(marc::frame::plan_lzmw_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, too_long, larger_workspace,
                  larger_staging)
                  .error,
              LzmwBlockedHuffmanFrameValidationError::input_size_mismatch);
}

} // namespace

#include "frame/lzd_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

using marc::frame::LzdBlockedHuffmanFrameValidationError;

constexpr std::array single_terminal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
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
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};

marc::frame::StreamHeader stream(const std::uint64_t size = 1) {
    marc::frame::StreamHeader result{};
    result.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    result.dictionary_variant = 1;
    result.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    result.entropy_variant = 1;
    result.frame_size = static_cast<std::uint32_t>(size);
    result.entropy_block_size = 8;
    result.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    result.original_size = size;
    return result;
}

TEST(LzdBlockedHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    const auto result = marc::frame::validate_lzd_blocked_huffman_frame(
        stream(), {}, {}, 0, 0, single_terminal_frame, views, staging, {});
    ASSERT_EQ(result.error, LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, 80U);
    EXPECT_EQ(result.dictionary_size, 8U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging[0], std::byte{'A'});
    EXPECT_EQ(staging[4], std::byte{0xff});
}

TEST(LzdBlockedHuffmanFrameValidator, RejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    for (std::size_t size = 0; size < single_terminal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzd_blocked_huffman_frame(
                      stream(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_terminal_frame}.first(
                          size),
                      views, staging, {})
                      .error,
                  LzdBlockedHuffmanFrameValidationError::none)
            << size;
    }
    auto trailing = single_terminal_frame;
    std::array<std::byte, single_terminal_frame.size() + 1> extended{};
    std::ranges::copy(trailing, extended.begin());
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, extended, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzdBlockedHuffmanFrameValidator, RejectsCallerWorkspaceShortages) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, {}, staging,
                  {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::view_output_too_small);
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  std::span<std::byte>{}, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);

    auto pair = single_terminal_frame;
    pair[16] = std::byte{2};
    pair[76] = std::byte{'B'};
    pair[77] = pair[78] = pair[79] = std::byte{};
    auto pair_stream = stream(2);
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  pair_stream, {}, {}, 0, 0, pair, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  phrase_workspace_too_small);
}

TEST(LzdBlockedHuffmanFrameValidator, EntropyThenDictionaryFailuresAreAtomic) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    auto bad_descriptor = single_terminal_frame;
    bad_descriptor[56] = std::byte{7};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_descriptor, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto bad_reference = single_terminal_frame;
    bad_reference[72] = std::byte{0};
    bad_reference[73] = std::byte{1};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_reference, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
}

TEST(LzdBlockedHuffmanFrameDecoder, PublishesOnlyAfterCompleteValidation) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    ASSERT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output[0], std::byte{'A'});

    output[0] = std::byte{0x5a};
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, {}, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::raw_output_too_small);
}

TEST(LzdBlockedHuffmanFrameDecoder, EnforcesCompleteAggregateAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes = 16 + 8 + 8 + sizeof(views)
        + sizeof(expansion) + output.size() - 1;
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, limits, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::workspace_limit);
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto unsupported = stream();
    unsupported.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  unsupported, {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(LzdBlockedHuffmanFrameEncoder, PlansAndEmitsSpecifiedHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 8> staging{};
    const auto plan = marc::frame::plan_lzd_blocked_huffman_frame(
        stream(), {}, {}, 0, 0, raw, {}, staging);
    ASSERT_EQ(plan.error, LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.raw_size, 1U);
    EXPECT_EQ(plan.dictionary_size, 8U);
    EXPECT_EQ(plan.encoder_entries, 0U);
    EXPECT_EQ(plan.dictionary_entries, 0U);
    EXPECT_EQ(plan.token_count, 1U);
    EXPECT_EQ(plan.descriptor_size, 16U);
    EXPECT_EQ(plan.payload_size, 8U);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.serialized_size, single_terminal_frame.size());

    std::array<std::byte, single_terminal_frame.size()> encoded{};
    ASSERT_EQ(marc::frame::encode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, raw, {}, staging, encoded)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(encoded, single_terminal_frame);
}

TEST(LzdBlockedHuffmanFrameEncoder,
     IsDeterministicAcrossTokenSplitsAndRoundTrips) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    auto configured_stream = stream(raw.size());
    configured_stream.entropy_block_size = 7;
    std::array<marc::dictionary::internal::LzdEncoderEntry, raw.size() / 2>
        encoder_workspace{};
    std::array<std::byte, 8 * ((raw.size() + 1) / 2)> encode_staging{};
    const auto plan = marc::frame::plan_lzd_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
        encode_staging);
    ASSERT_EQ(plan.error, LzdBlockedHuffmanFrameValidationError::none);
    ASSERT_GT(plan.block_count, 1U);
    ASSERT_EQ(configured_stream.entropy_block_size, 7U);

    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, first)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, second)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
        plan.block_count);
    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::vector<marc::dictionary::internal::LzdPhraseEntry> phrases(
        plan.encoder_entries);
    std::vector<std::uint32_t> expansion(plan.dictionary_entries + 1U);
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, first, views,
                  decode_staging, phrases, expansion, decoded)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzdBlockedHuffmanFrameEncoder, UsesCanonicalHuffmanWhenSmaller) {
    std::array<std::byte, 1024> raw{};
    std::uint32_t state = 0x9e3779b9U;
    for (auto& value : raw) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        value = static_cast<std::byte>(state & 0xffU);
    }
    auto configured_stream = stream(raw.size());
    configured_stream.entropy_block_size = 8192;
    std::array<marc::dictionary::internal::LzdEncoderEntry, raw.size() / 2>
        encoder_workspace{};
    std::array<std::byte, 8 * ((raw.size() + 1) / 2)> encode_staging{};
    const auto plan = marc::frame::plan_lzd_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
        encode_staging);
    ASSERT_EQ(plan.error, LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.descriptor_size, 272U);
    EXPECT_LT(plan.payload_size, plan.dictionary_size);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, encoder_workspace,
                  encode_staging, encoded)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::vector<marc::dictionary::internal::LzdPhraseEntry> phrases(
        plan.encoder_entries);
    std::vector<std::uint32_t> expansion(plan.dictionary_entries + 1U);
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, encoded, views,
                  decode_staging, phrases, expansion, decoded)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(views[0].descriptor.flags, 0U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzdBlockedHuffmanFrameEncoder,
     RejectsWorkspaceAndOutputCapacityAtomically) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    auto configured_stream = stream(raw.size());
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, {}, staging)
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> workspace{};
    EXPECT_EQ(marc::frame::plan_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, raw, workspace, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);

    std::array<std::byte, single_terminal_frame.size() - 1> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lzd_blocked_huffman_frame(
        configured_stream, {}, {}, 0, 0, raw, workspace, staging,
        short_output);
    EXPECT_EQ(result.error, LzdBlockedHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, single_terminal_frame.size());
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzdBlockedHuffmanFrameEncoder,
     EnforcesAggregateWorkspaceAndFrameExtent) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    auto configured_stream = stream(raw.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> workspace{};
    std::array<std::byte, 8> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes =
        sizeof(workspace) + staging.size() - 1;
    EXPECT_EQ(marc::frame::plan_lzd_blocked_huffman_frame(
                  configured_stream, {}, limits, 0, 0, raw, workspace,
                  staging)
                  .error,
              LzdBlockedHuffmanFrameValidationError::workspace_limit);

    EXPECT_EQ(marc::frame::plan_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0,
                  std::span<const std::byte>{}, workspace, staging)
                  .error,
              LzdBlockedHuffmanFrameValidationError::input_size_mismatch);
    constexpr std::array too_long{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    std::array<std::byte, 16> larger_staging{};
    EXPECT_EQ(marc::frame::plan_lzd_blocked_huffman_frame(
                  configured_stream, {}, {}, 0, 0, too_long, workspace,
                  larger_staging)
                  .error,
              LzdBlockedHuffmanFrameValidationError::input_size_mismatch);
}

} // namespace

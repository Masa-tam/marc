#include "frame/lz78_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz78BlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
  marc::frame::StreamHeader stream{};
  stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
  stream.dictionary_variant = 1;
  stream.entropy_algorithm = marc::frame::EntropyAlgorithm::blocked_huffman;
  stream.entropy_variant = 1;
  stream.frame_size = 1;
  stream.entropy_block_size = 8;
  stream.dictionary_parameters_size =
      marc::dictionary::internal::lz78_parameter_size;
  stream.original_size = 1;
  return stream;
}

constexpr std::array<std::byte, 80> single_pair_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{8},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{8},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0x10}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{8},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{8},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{1},    std::byte{8},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0x41}, std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0}};

} // namespace

TEST(Lz78BlockedHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  const auto result = marc::frame::validate_lz78_blocked_huffman_frame(
      stream_for_a(), {}, {}, 0, 0, single_pair_frame, views, staging, phrases);
  ASSERT_EQ(result.error, Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(result.serialized_size, single_pair_frame.size());
  EXPECT_EQ(result.dictionary_size, 8U);
  EXPECT_EQ(result.raw_size, 1U);
  EXPECT_EQ(result.block_count, 1U);
  EXPECT_EQ(result.phrase_entries, 1U);
  EXPECT_EQ(staging[0], std::byte{0});
  EXPECT_EQ(staging[1], std::byte{'A'});
  EXPECT_EQ(phrases[0].prefix_index, 0U);
  EXPECT_EQ(phrases[0].symbol, static_cast<std::uint8_t>('A'));
  EXPECT_EQ(phrases[0].length, 1U);
}

TEST(Lz78BlockedHuffmanFrameValidator,
     StrictlyRejectsTruncationAndTrailingBytes) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  for (std::size_t size = 0; size < single_pair_frame.size(); ++size) {
    EXPECT_NE(marc::frame::validate_lz78_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0,
                  std::span<const std::byte>{single_pair_frame}.first(size),
                  views, staging, phrases)
                  .error,
              Lz78BlockedHuffmanFrameValidationError::none)
        << size;
  }
  std::vector<std::byte> extended(single_pair_frame.begin(),
                                  single_pair_frame.end());
  extended.push_back(std::byte{0});
  EXPECT_EQ(marc::frame::validate_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, extended, views, staging, phrases)
                .error,
            Lz78BlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(Lz78BlockedHuffmanFrameValidator,
     RejectsEveryIndependentWorkspaceShortageBeforeEntropyOutput) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  staging.fill(std::byte{0x5a});
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};

  EXPECT_EQ(marc::frame::validate_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, single_pair_frame,
                std::span<marc::entropy::internal::BlockedHuffmanBlockView>{},
                staging, phrases)
                .error,
            Lz78BlockedHuffmanFrameValidationError::view_output_too_small);
  EXPECT_EQ(
      marc::frame::validate_lz78_blocked_huffman_frame(
          stream_for_a(), {}, {}, 0, 0, single_pair_frame, views,
          std::span<std::byte>{}, phrases)
          .error,
      Lz78BlockedHuffmanFrameValidationError::dictionary_staging_too_small);
  EXPECT_EQ(marc::frame::validate_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, single_pair_frame, views, staging,
                std::span<marc::dictionary::internal::Lz78PhraseEntry>{})
                .error,
            Lz78BlockedHuffmanFrameValidationError::phrase_workspace_too_small);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(Lz78BlockedHuffmanFrameValidator, CountsBothTypedWorkspaceRegions) {
  auto limits = marc::core::DecoderLimits{};
  limits.max_block_size = 8;
  const std::uint64_t required =
      16 + 8 + 8 + sizeof(marc::entropy::internal::BlockedHuffmanBlockView) +
      sizeof(marc::dictionary::internal::Lz78PhraseEntry);
  limits.max_internal_buffered_bytes = required - 1;
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  EXPECT_EQ(marc::frame::validate_lz78_blocked_huffman_frame(
                stream_for_a(), {}, limits, 0, 0, single_pair_frame, views,
                staging, phrases)
                .error,
            Lz78BlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(Lz78BlockedHuffmanFrameValidator,
     RejectsEntropyMetadataBeforeWritingStaging) {
  auto malformed = single_pair_frame;
  malformed[64] = std::byte{1};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  staging.fill(std::byte{0x5a});
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  const auto result = marc::frame::validate_lz78_blocked_huffman_frame(
      stream_for_a(), {}, {}, 0, 0, malformed, views, staging, phrases);
  EXPECT_EQ(result.error,
            Lz78BlockedHuffmanFrameValidationError::controller_error);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(Lz78BlockedHuffmanFrameValidator, RejectsInvalidPhraseReference) {
  auto malformed = single_pair_frame;
  malformed[76] = std::byte{1};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  const auto result = marc::frame::validate_lz78_blocked_huffman_frame(
      stream_for_a(), {}, {}, 0, 0, malformed, views, staging, phrases);
  EXPECT_EQ(
      result.error,
      Lz78BlockedHuffmanFrameValidationError::dictionary_validation_error);
  EXPECT_EQ(result.dictionary_error,
            marc::dictionary::internal::Lz78ValidationError::token_error);
  EXPECT_EQ(result.dictionary_format_error,
            marc::dictionary::internal::Lz78FormatError::invalid_phrase_index);
}

TEST(Lz78BlockedHuffmanFrameDecoder, DecodesOnlyAfterCompleteValidation) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  std::array<std::byte, 3> output{};
  output.fill(std::byte{0x5a});
  ASSERT_EQ(marc::frame::decode_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, single_pair_frame, views, staging,
                phrases, output)
                .error,
            Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(output[0], std::byte{'A'});
  EXPECT_EQ(output[1], std::byte{0x5a});
  EXPECT_EQ(output[2], std::byte{0x5a});

  auto malformed = single_pair_frame;
  malformed[72] = std::byte{0xff};
  output[0] = std::byte{0x5a};
  EXPECT_EQ(
      marc::frame::decode_lz78_blocked_huffman_frame(stream_for_a(), {}, {}, 0,
                                                     0, malformed, views,
                                                     staging, phrases, output)
          .error,
      Lz78BlockedHuffmanFrameValidationError::dictionary_validation_error);
  EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(Lz78BlockedHuffmanFrameDecoder, ShortOutputIsAtomic) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  std::array<std::byte, 0> output{};
  EXPECT_EQ(marc::frame::decode_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, single_pair_frame, views, staging,
                phrases, output)
                .error,
            Lz78BlockedHuffmanFrameValidationError::raw_output_too_small);
}

TEST(Lz78BlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 8> staging{};
  std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
  EXPECT_EQ(marc::frame::validate_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 1, 0, single_pair_frame, views, staging,
                phrases)
                .error,
            Lz78BlockedHuffmanFrameValidationError::header_error);
  auto stream = stream_for_a();
  stream.dictionary_variant = 0;
  EXPECT_EQ(
      marc::frame::validate_lz78_blocked_huffman_frame(
          stream, {}, {}, 0, 0, single_pair_frame, views, staging, phrases)
          .error,
      Lz78BlockedHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(Lz78BlockedHuffmanFrameEncoder, PlansAndEmitsSpecifiedHandVector) {
  const std::array raw{std::byte{'A'}};
  std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> workspace{};
  std::array<std::byte, 8> staging{};
  const auto plan = marc::frame::plan_lz78_blocked_huffman_frame(
      stream_for_a(), {}, {}, 0, 0, raw, workspace, staging);
  ASSERT_EQ(plan.error, Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(plan.raw_size, 1U);
  EXPECT_EQ(plan.dictionary_size, 8U);
  EXPECT_EQ(plan.encoder_entries, 1U);
  EXPECT_EQ(plan.descriptor_size, 16U);
  EXPECT_EQ(plan.payload_size, 8U);
  EXPECT_EQ(plan.block_count, 1U);
  EXPECT_EQ(plan.serialized_size, single_pair_frame.size());

  std::array<std::byte, single_pair_frame.size()> encoded{};
  ASSERT_EQ(marc::frame::encode_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, raw, workspace, staging, encoded)
                .error,
            Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(encoded, single_pair_frame);
}

TEST(Lz78BlockedHuffmanFrameEncoder,
     IsDeterministicAcrossMultipleEntropyBlocksAndRoundTrips) {
  constexpr std::array raw{std::byte{'A'}, std::byte{'A'}, std::byte{'B'},
                           std::byte{'A'}, std::byte{'B'}, std::byte{'C'},
                           std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
  auto stream = stream_for_a();
  stream.frame_size = static_cast<std::uint32_t>(raw.size());
  stream.original_size = raw.size();
  stream.entropy_block_size = 16;
  std::array<marc::dictionary::internal::Lz78EncoderEntry, raw.size()>
      encoder_workspace{};
  std::array<std::byte, raw.size() * 8> encode_staging{};
  const auto plan = marc::frame::plan_lz78_blocked_huffman_frame(
      stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging);
  ASSERT_EQ(plan.error, Lz78BlockedHuffmanFrameValidationError::none);
  ASSERT_GT(plan.block_count, 1U);

  std::vector<std::byte> first(plan.serialized_size);
  std::vector<std::byte> second(plan.serialized_size);
  ASSERT_EQ(
      marc::frame::encode_lz78_blocked_huffman_frame(
          stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging, first)
          .error,
      Lz78BlockedHuffmanFrameValidationError::none);
  ASSERT_EQ(
      marc::frame::encode_lz78_blocked_huffman_frame(
          stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging, second)
          .error,
      Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(first, second);

  std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
      plan.block_count);
  std::vector<std::byte> decode_staging(plan.dictionary_size);
  std::vector<marc::dictionary::internal::Lz78PhraseEntry> phrases(
      plan.encoder_entries);
  std::array<std::byte, raw.size()> decoded{};
  ASSERT_EQ(
      marc::frame::decode_lz78_blocked_huffman_frame(
          stream, {}, {}, 0, 0, first, views, decode_staging, phrases, decoded)
          .error,
      Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(decoded, raw);
}

TEST(Lz78BlockedHuffmanFrameEncoder, UsesCanonicalHuffmanWhenSmaller) {
  std::array<std::byte, 1024> raw{};
  std::array<marc::dictionary::internal::Lz78EncoderEntry, raw.size()>
      encoder_workspace{};
  std::array<std::byte, raw.size() * 8> staging{};
  auto stream = stream_for_a();
  stream.frame_size = static_cast<std::uint32_t>(raw.size());
  stream.original_size = raw.size();
  stream.entropy_block_size = 8192;
  const auto plan = marc::frame::plan_lz78_blocked_huffman_frame(
      stream, {}, {}, 0, 0, raw, encoder_workspace, staging);
  ASSERT_EQ(plan.error, Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(plan.block_count, 1U);
  EXPECT_EQ(plan.descriptor_size, 272U);
  EXPECT_LT(plan.payload_size, plan.dictionary_size);

  std::vector<std::byte> encoded(plan.serialized_size);
  ASSERT_EQ(marc::frame::encode_lz78_blocked_huffman_frame(
                stream, {}, {}, 0, 0, raw, encoder_workspace, staging, encoded)
                .error,
            Lz78BlockedHuffmanFrameValidationError::none);
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::vector<std::byte> decode_staging(plan.dictionary_size);
  std::vector<marc::dictionary::internal::Lz78PhraseEntry> phrases(
      plan.encoder_entries);
  std::array<std::byte, raw.size()> decoded{};
  ASSERT_EQ(marc::frame::decode_lz78_blocked_huffman_frame(
                stream, {}, {}, 0, 0, encoded, views, decode_staging, phrases,
                decoded)
                .error,
            Lz78BlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(views[0].descriptor.flags, 0U);
  EXPECT_EQ(decoded, raw);
}

TEST(Lz78BlockedHuffmanFrameEncoder,
     RejectsWorkspaceAndOutputCapacityAtomically) {
  const std::array raw{std::byte{'A'}};
  std::array<std::byte, 8> staging{};
  staging.fill(std::byte{0x5a});
  EXPECT_EQ(
      marc::frame::plan_lz78_blocked_huffman_frame(
          stream_for_a(), {}, {}, 0, 0, raw,
          std::span<marc::dictionary::internal::Lz78EncoderEntry>{}, staging)
          .error,
      Lz78BlockedHuffmanFrameValidationError::encoder_workspace_too_small);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));

  std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> workspace{};
  EXPECT_EQ(
      marc::frame::plan_lz78_blocked_huffman_frame(
          stream_for_a(), {}, {}, 0, 0, raw, workspace, std::span<std::byte>{})
          .error,
      Lz78BlockedHuffmanFrameValidationError::dictionary_staging_too_small);

  std::array<std::byte, single_pair_frame.size() - 1> short_output{};
  short_output.fill(std::byte{0x5a});
  const auto result = marc::frame::encode_lz78_blocked_huffman_frame(
      stream_for_a(), {}, {}, 0, 0, raw, workspace, staging, short_output);
  EXPECT_EQ(
      result.error,
      Lz78BlockedHuffmanFrameValidationError::serialized_output_too_small);
  EXPECT_EQ(result.serialized_size, single_pair_frame.size());
  EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
    return value == std::byte{0x5a};
  }));
}

TEST(Lz78BlockedHuffmanFrameEncoder, EnforcesAggregateWorkspaceAndFrameExtent) {
  const std::array raw{std::byte{'A'}};
  std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> workspace{};
  std::array<std::byte, 8> staging{};
  auto limits = marc::core::DecoderLimits{};
  limits.max_block_size = 8;
  limits.max_internal_buffered_bytes = sizeof(workspace) + staging.size() - 1;
  EXPECT_EQ(marc::frame::plan_lz78_blocked_huffman_frame(
                stream_for_a(), {}, limits, 0, 0, raw, workspace, staging)
                .error,
            Lz78BlockedHuffmanFrameValidationError::workspace_limit);

  EXPECT_EQ(marc::frame::plan_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, std::span<const std::byte>{},
                workspace, staging)
                .error,
            Lz78BlockedHuffmanFrameValidationError::input_size_mismatch);
  const std::array too_long{std::byte{'A'}, std::byte{'B'}};
  std::array<marc::dictionary::internal::Lz78EncoderEntry, 2>
      larger_workspace{};
  std::array<std::byte, 16> larger_staging{};
  EXPECT_EQ(marc::frame::plan_lz78_blocked_huffman_frame(
                stream_for_a(), {}, {}, 0, 0, too_long, larger_workspace,
                larger_staging)
                .error,
            Lz78BlockedHuffmanFrameValidationError::input_size_mismatch);
}

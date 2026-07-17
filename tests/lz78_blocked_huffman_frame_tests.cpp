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

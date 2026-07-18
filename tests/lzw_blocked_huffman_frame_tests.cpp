#include "frame/lzw_blocked_huffman_frame.hpp"

#include "entropy/blocked_huffman_frame_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzwBlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader
stream_for(const std::uint32_t raw_size, const std::uint32_t block_size) {
  marc::frame::StreamHeader stream{};
  stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzw;
  stream.dictionary_variant = 1;
  stream.entropy_algorithm = marc::frame::EntropyAlgorithm::blocked_huffman;
  stream.entropy_variant = 1;
  stream.frame_size = raw_size;
  stream.entropy_block_size = block_size;
  stream.dictionary_parameters_size =
      marc::dictionary::internal::lzw_parameter_size;
  stream.original_size = raw_size;
  return stream;
}

constexpr std::array<std::byte, 74> single_code_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{2},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{2},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0x10}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{2},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{2},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{1},    std::byte{8},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0x41}, std::byte{0}};

constexpr std::array<std::byte, 75> two_code_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{2},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{3},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{3},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0x10}, std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{3},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{3},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0},    std::byte{0},    std::byte{1},    std::byte{8},
    std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
    std::byte{0x41}, std::byte{0x82}, std::byte{0}};

[[nodiscard]] std::vector<std::byte>
width_boundary_frame(marc::frame::StreamHeader &stream) {
  constexpr std::size_t raw_size = 259;
  std::vector<std::byte> codes(291);
  codes.back() = std::byte{0x08};
  stream = stream_for(raw_size, 10);
  const auto plan = marc::entropy::internal::plan_blocked_huffman_frame(
      codes, stream.entropy_block_size, {});
  EXPECT_EQ(plan.error,
            marc::entropy::internal::BlockedHuffmanFrameEncodeError::none);

  marc::frame::FrameHeader header{};
  header.uncompressed_size = raw_size;
  header.dictionary_serialized_size = static_cast<std::uint32_t>(codes.size());
  header.compressed_payload_size =
      static_cast<std::uint32_t>(plan.payload_size);
  header.entropy_block_count = static_cast<std::uint32_t>(plan.block_count);
  header.block_descriptors_size =
      static_cast<std::uint32_t>(plan.descriptor_region_size);
  std::vector<std::byte> frame(marc::frame::frame_header_size +
                               plan.descriptor_region_size + plan.payload_size);
  const marc::frame::FrameValidationContext context{stream, {}, 0, 0};
  const std::span<std::byte, marc::frame::frame_header_size> header_output{
      frame.data(), marc::frame::frame_header_size};
  EXPECT_EQ(marc::frame::serialize_frame_header(header, context, header_output),
            marc::frame::FrameHeaderError::none);
  const auto encoded = marc::entropy::internal::encode_blocked_huffman_frame(
      codes, stream.entropy_block_size, {},
      std::span{frame}.subspan(marc::frame::frame_header_size,
                               plan.descriptor_region_size),
      std::span{frame}.subspan(marc::frame::frame_header_size +
                                   plan.descriptor_region_size,
                               plan.payload_size));
  EXPECT_EQ(encoded.error,
            marc::entropy::internal::BlockedHuffmanFrameEncodeError::none);
  return frame;
}

} // namespace

TEST(LzwBlockedHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  const auto result = marc::frame::validate_lzw_blocked_huffman_frame(
      stream_for(1, 2), {}, {}, 0, 0, single_code_frame, views, staging, {});
  ASSERT_EQ(result.error, LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(result.serialized_size, single_code_frame.size());
  EXPECT_EQ(result.dictionary_size, 2U);
  EXPECT_EQ(result.raw_size, 1U);
  EXPECT_EQ(result.block_count, 1U);
  EXPECT_EQ(result.phrase_entries, 0U);
  EXPECT_EQ(staging[0], std::byte{0x41});
  EXPECT_EQ(staging[1], std::byte{0});
}

TEST(LzwBlockedHuffmanFrameValidator,
     StrictlyRejectsTruncationAndTrailingBytes) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  for (std::size_t size = 0; size < single_code_frame.size(); ++size) {
    EXPECT_NE(marc::frame::validate_lzw_blocked_huffman_frame(
                  stream_for(1, 2), {}, {}, 0, 0,
                  std::span<const std::byte>{single_code_frame}.first(size),
                  views, staging, {})
                  .error,
              LzwBlockedHuffmanFrameValidationError::none)
        << size;
  }
  std::vector<std::byte> extended(single_code_frame.begin(),
                                  single_code_frame.end());
  extended.push_back(std::byte{0});
  EXPECT_EQ(marc::frame::validate_lzw_blocked_huffman_frame(
                stream_for(1, 2), {}, {}, 0, 0, extended, views, staging, {})
                .error,
            LzwBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzwBlockedHuffmanFrameValidator,
     RejectsEveryIndependentWorkspaceShortageBeforeEntropyOutput) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  staging.fill(std::byte{0x5a});
  EXPECT_EQ(
      marc::frame::validate_lzw_blocked_huffman_frame(
          stream_for(1, 2), {}, {}, 0, 0, single_code_frame, {}, staging, {})
          .error,
      LzwBlockedHuffmanFrameValidationError::view_output_too_small);
  EXPECT_EQ(
      marc::frame::validate_lzw_blocked_huffman_frame(
          stream_for(1, 2), {}, {}, 0, 0, single_code_frame, views, {}, {})
          .error,
      LzwBlockedHuffmanFrameValidationError::dictionary_staging_too_small);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));

  std::array<std::byte, 3> two_staging{};
  std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
  ASSERT_EQ(marc::frame::validate_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, {}, 0, 0, two_code_frame, views,
                two_staging, phrases)
                .error,
            LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(marc::frame::validate_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, {}, 0, 0, two_code_frame, views,
                two_staging, {})
                .error,
            LzwBlockedHuffmanFrameValidationError::phrase_workspace_too_small);
}

TEST(LzwBlockedHuffmanFrameValidator, CountsBothTypedWorkspaceRegions) {
  auto limits = marc::core::DecoderLimits{};
  limits.max_block_size = 2;
  const std::uint64_t required =
      16 + 2 + 2 + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
  limits.max_internal_buffered_bytes = required - 1;
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  EXPECT_EQ(marc::frame::validate_lzw_blocked_huffman_frame(
                stream_for(1, 2), {}, limits, 0, 0, single_code_frame, views,
                staging, {})
                .error,
            LzwBlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(LzwBlockedHuffmanFrameValidator,
     RejectsEntropyMetadataBeforeWritingStaging) {
  auto malformed = single_code_frame;
  malformed[56] = std::byte{1};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  staging.fill(std::byte{0x5a});
  const auto result = marc::frame::validate_lzw_blocked_huffman_frame(
      stream_for(1, 2), {}, {}, 0, 0, malformed, views, staging, {});
  EXPECT_EQ(result.error,
            LzwBlockedHuffmanFrameValidationError::controller_error);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzwBlockedHuffmanFrameValidator, RejectsNonzeroLzwPaddingAfterEntropy) {
  auto malformed = single_code_frame;
  malformed[73] = std::byte{0x80};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  const auto result = marc::frame::validate_lzw_blocked_huffman_frame(
      stream_for(1, 2), {}, {}, 0, 0, malformed, views, staging, {});
  EXPECT_EQ(result.error,
            LzwBlockedHuffmanFrameValidationError::dictionary_validation_error);
  EXPECT_EQ(result.dictionary_error,
            marc::dictionary::internal::LzwValidationError::code_error);
  EXPECT_EQ(result.dictionary_format_error,
            marc::dictionary::internal::LzwFormatError::nonzero_padding);
}

TEST(LzwBlockedHuffmanFrameValidator,
     AcceptsWidthChangeAcrossIndependentEntropyBlocks) {
  marc::frame::StreamHeader stream{};
  const auto frame = width_boundary_frame(stream);
  std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(30);
  std::vector<std::byte> staging(291);
  std::vector<marc::dictionary::internal::LzwPhraseEntry> phrases(
      marc::dictionary::internal::lzw_validation_workspace_entries(291, {}));
  const auto result = marc::frame::validate_lzw_blocked_huffman_frame(
      stream, {}, {}, 0, 0, frame, views, staging, phrases);
  ASSERT_EQ(result.error, LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(result.block_count, 30U);
  EXPECT_EQ(result.raw_size, 259U);
  std::array<std::byte, 259> decoded{};
  EXPECT_EQ(marc::frame::decode_lzw_blocked_huffman_frame(
                stream, {}, {}, 0, 0, frame, views, staging, phrases, decoded)
                .error,
            LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_TRUE(std::ranges::all_of(
      decoded, [](const std::byte value) { return value == std::byte{0}; }));
}

TEST(LzwBlockedHuffmanFrameDecoder, DecodesOnlyAfterCompleteValidation) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  std::array<std::byte, 3> output{};
  output.fill(std::byte{0x5a});
  ASSERT_EQ(marc::frame::decode_lzw_blocked_huffman_frame(
                stream_for(1, 2), {}, {}, 0, 0, single_code_frame, views,
                staging, {}, output)
                .error,
            LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(output[0], std::byte{'A'});
  EXPECT_EQ(output[1], std::byte{0x5a});

  auto malformed = single_code_frame;
  malformed[73] = std::byte{0x80};
  output[0] = std::byte{0x5a};
  EXPECT_EQ(
      marc::frame::decode_lzw_blocked_huffman_frame(
          stream_for(1, 2), {}, {}, 0, 0, malformed, views, staging, {}, output)
          .error,
      LzwBlockedHuffmanFrameValidationError::dictionary_validation_error);
  EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(LzwBlockedHuffmanFrameDecoder, ShortOutputIsAtomic) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  EXPECT_EQ(marc::frame::decode_lzw_blocked_huffman_frame(
                stream_for(1, 2), {}, {}, 0, 0, single_code_frame, views,
                staging, {}, {})
                .error,
            LzwBlockedHuffmanFrameValidationError::raw_output_too_small);
}

TEST(LzwBlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<std::byte, 2> staging{};
  EXPECT_EQ(
      marc::frame::validate_lzw_blocked_huffman_frame(
          stream_for(1, 2), {}, {}, 1, 0, single_code_frame, views, staging, {})
          .error,
      LzwBlockedHuffmanFrameValidationError::header_error);
  auto stream = stream_for(1, 2);
  stream.dictionary_variant = 0;
  EXPECT_EQ(marc::frame::validate_lzw_blocked_huffman_frame(
                stream, {}, {}, 0, 0, single_code_frame, views, staging, {})
                .error,
            LzwBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(LzwBlockedHuffmanFrameEncoder, PlansAndEmitsSpecifiedHandVector) {
  const std::array raw{std::byte{'A'}};
  std::array<std::byte, 2> staging{};
  const auto plan = marc::frame::plan_lzw_blocked_huffman_frame(
      stream_for(1, 2), {}, {}, 0, 0, raw, {}, staging);
  ASSERT_EQ(plan.error, LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(plan.raw_size, 1U);
  EXPECT_EQ(plan.dictionary_size, 2U);
  EXPECT_EQ(plan.encoder_entries, 0U);
  EXPECT_EQ(plan.code_count, 1U);
  EXPECT_EQ(plan.descriptor_size, 16U);
  EXPECT_EQ(plan.payload_size, 2U);
  EXPECT_EQ(plan.block_count, 1U);
  EXPECT_EQ(plan.serialized_size, single_code_frame.size());

  std::array<std::byte, single_code_frame.size()> encoded{};
  ASSERT_EQ(marc::frame::encode_lzw_blocked_huffman_frame(
                stream_for(1, 2), {}, {}, 0, 0, raw, {}, staging, encoded)
                .error,
            LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(encoded, single_code_frame);
}

TEST(LzwBlockedHuffmanFrameEncoder,
     IsDeterministicAcrossMultipleEntropyBlocksAndRoundTrips) {
  constexpr std::array raw{std::byte{'A'}, std::byte{'A'}, std::byte{'B'},
                           std::byte{'A'}, std::byte{'B'}, std::byte{'C'},
                           std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
  auto stream = stream_for(static_cast<std::uint32_t>(raw.size()), 2);
  std::array<marc::dictionary::internal::LzwEncoderEntry, raw.size() - 1>
      encoder_workspace{};
  std::array<std::byte, raw.size() * 2> encode_staging{};
  const auto plan = marc::frame::plan_lzw_blocked_huffman_frame(
      stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging);
  ASSERT_EQ(plan.error, LzwBlockedHuffmanFrameValidationError::none);
  ASSERT_GT(plan.block_count, 1U);

  std::vector<std::byte> first(plan.serialized_size);
  std::vector<std::byte> second(plan.serialized_size);
  ASSERT_EQ(
      marc::frame::encode_lzw_blocked_huffman_frame(
          stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging, first)
          .error,
      LzwBlockedHuffmanFrameValidationError::none);
  ASSERT_EQ(
      marc::frame::encode_lzw_blocked_huffman_frame(
          stream, {}, {}, 0, 0, raw, encoder_workspace, encode_staging, second)
          .error,
      LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(first, second);

  std::vector<marc::entropy::internal::BlockedHuffmanBlockView> views(
      plan.block_count);
  std::vector<std::byte> decode_staging(plan.dictionary_size);
  std::vector<marc::dictionary::internal::LzwPhraseEntry> phrases(
      marc::dictionary::internal::lzw_validation_workspace_entries(
          plan.dictionary_size, {}));
  std::array<std::byte, raw.size()> decoded{};
  ASSERT_EQ(
      marc::frame::decode_lzw_blocked_huffman_frame(
          stream, {}, {}, 0, 0, first, views, decode_staging, phrases, decoded)
          .error,
      LzwBlockedHuffmanFrameValidationError::none);
  EXPECT_EQ(decoded, raw);
}

TEST(LzwBlockedHuffmanFrameEncoder,
     RejectsWorkspaceAndOutputCapacityAtomically) {
  const std::array raw{std::byte{'A'}, std::byte{'A'}};
  std::array<std::byte, 3> staging{};
  staging.fill(std::byte{0x5a});
  EXPECT_EQ(marc::frame::plan_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, {}, 0, 0, raw,
                std::span<marc::dictionary::internal::LzwEncoderEntry>{},
                staging)
                .error,
            LzwBlockedHuffmanFrameValidationError::encoder_workspace_too_small);
  EXPECT_TRUE(std::ranges::all_of(
      staging, [](const std::byte value) { return value == std::byte{0x5a}; }));

  std::array<marc::dictionary::internal::LzwEncoderEntry, 1> workspace{};
  EXPECT_EQ(
      marc::frame::plan_lzw_blocked_huffman_frame(stream_for(2, 3), {}, {}, 0,
                                                  0, raw, workspace,
                                                  std::span<std::byte>{})
          .error,
      LzwBlockedHuffmanFrameValidationError::dictionary_staging_too_small);

  std::array<std::byte, two_code_frame.size() - 1> short_output{};
  short_output.fill(std::byte{0x5a});
  const auto result = marc::frame::encode_lzw_blocked_huffman_frame(
      stream_for(2, 3), {}, {}, 0, 0, raw, workspace, staging, short_output);
  EXPECT_EQ(result.error,
            LzwBlockedHuffmanFrameValidationError::serialized_output_too_small);
  EXPECT_EQ(result.serialized_size, two_code_frame.size());
  EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
    return value == std::byte{0x5a};
  }));
}

TEST(LzwBlockedHuffmanFrameEncoder, EnforcesAggregateWorkspaceAndFrameExtent) {
  const std::array raw{std::byte{'A'}, std::byte{'A'}};
  std::array<marc::dictionary::internal::LzwEncoderEntry, 1> workspace{};
  std::array<std::byte, 3> staging{};
  auto limits = marc::core::DecoderLimits{};
  limits.max_block_size = 3;
  limits.max_internal_buffered_bytes = sizeof(workspace) + staging.size() - 1;
  EXPECT_EQ(marc::frame::plan_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, limits, 0, 0, raw, workspace, staging)
                .error,
            LzwBlockedHuffmanFrameValidationError::workspace_limit);

  EXPECT_EQ(marc::frame::plan_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, {}, 0, 0, std::span<const std::byte>{},
                workspace, staging)
                .error,
            LzwBlockedHuffmanFrameValidationError::input_size_mismatch);
  const std::array too_long{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
  std::array<marc::dictionary::internal::LzwEncoderEntry, 2> larger_workspace{};
  std::array<std::byte, 6> larger_staging{};
  EXPECT_EQ(marc::frame::plan_lzw_blocked_huffman_frame(
                stream_for(2, 3), {}, {}, 0, 0, too_long, larger_workspace,
                larger_staging)
                .error,
            LzwBlockedHuffmanFrameValidationError::input_size_mismatch);
}

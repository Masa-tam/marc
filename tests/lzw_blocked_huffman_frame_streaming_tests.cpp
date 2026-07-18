#include "frame/lzw_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_blocked_huffman_profile.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array raw{std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
                         std::byte{'B'}, std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
  StreamHeader stream{};
  stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
  stream.dictionary_variant = 1;
  stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
  stream.entropy_variant = 1;
  stream.frame_size = 2;
  stream.entropy_block_size = 4;
  stream.dictionary_parameters_size =
      marc::dictionary::internal::lzw_parameter_size;
  stream.original_size = size;
  return stream;
}

[[nodiscard]] std::vector<std::byte> encoded_stream() {
  const auto stream = config(raw.size());
  std::vector<std::byte> encoded(lzw_blocked_huffman_stream_prefix_size);
  const std::span<std::byte, stream_header_size> header{encoded.data(),
                                                        stream_header_size};
  const std::span<std::byte, marc::dictionary::internal::lzw_parameter_size>
      parameters{encoded.data() + stream_header_size,
                 marc::dictionary::internal::lzw_parameter_size};
  EXPECT_EQ(serialize_stream_header(stream, {}, header),
            StreamHeaderError::none);
  EXPECT_EQ(
      marc::dictionary::internal::serialize_lzw_parameters({}, {}, parameters),
      marc::dictionary::internal::LzwFormatError::none);

  std::array<marc::dictionary::internal::LzwEncoderEntry, 1> entries{};
  std::array<std::byte, 4> staging{};
  std::uint64_t sequence{};
  for (std::size_t offset = 0; offset < raw.size(); offset += 2) {
    const auto input = std::span<const std::byte>{raw}.subspan(
        offset, std::min<std::size_t>(2, raw.size() - offset));
    const auto plan = plan_lzw_blocked_huffman_frame(
        stream, {}, {}, sequence, offset, input, entries, staging);
    EXPECT_EQ(plan.error, LzwBlockedHuffmanFrameValidationError::none);
    const auto original_size = encoded.size();
    encoded.resize(original_size + plan.serialized_size);
    EXPECT_EQ(encode_lzw_blocked_huffman_frame(
                  stream, {}, {}, sequence, offset, input, entries, staging,
                  std::span<std::byte>{encoded}.subspan(original_size))
                  .error,
              LzwBlockedHuffmanFrameValidationError::none);
    ++sequence;
  }
  return encoded;
}

} // namespace

TEST(LzwBlockedHuffmanFrameStreaming, ProfileWorkspacesAreDirectlyUsable) {
  auto limits = marc::core::DecoderLimits{};
  limits.max_total_output_size = raw.size();
  limits.max_frame_size = 2;
  limits.max_block_size = 4;
  limits.max_compressed_payload_size = 4;
  limits.max_dictionary_serialized_size = 4;
  limits.max_dictionary_entries = 65536;
  limits.max_blocks_per_frame = 1;
  limits.max_internal_buffered_bytes = 512;

  StreamHeader stream{};
  LzwBlockedHuffmanEncoderWorkspaceRequirements encoder_requirements{};
  const LzwBlockedHuffmanProfileConfig profile_config{raw.size(), 2, 4, {}};
  ASSERT_EQ(make_lzw_blocked_huffman_profile(profile_config, limits, stream,
                                             encoder_requirements),
            LzwBlockedHuffmanProfileError::none);
  std::vector<std::byte> frame_input(encoder_requirements.frame_input_bytes);
  std::vector<std::byte> staging(encoder_requirements.dictionary_staging_bytes);
  std::vector<std::byte> frame_encoded(
      encoder_requirements.frame_encoded_bytes);
  std::vector<std::byte> opaque(encoder_requirements.views_bytes);
  LzwBlockedHuffmanEncoderViews encoder_views{};
  ASSERT_EQ(partition_lzw_blocked_huffman_encoder_views(encoder_requirements,
                                                        opaque, encoder_views),
            LzwBlockedHuffmanWorkspaceError::none);
  LzwBlockedHuffmanFrameStreamingEncoder encoder{
      stream,        profile_config.parameters, limits, frame_input, staging,
      frame_encoded, encoder_views.entries};
  EXPECT_NE(encoder.process({}, {}, 0).status, marc::core::StreamStatus::error);

  LzwBlockedHuffmanDecoderWorkspaceRequirements decoder_requirements{};
  ASSERT_EQ(calculate_lzw_blocked_huffman_decoder_workspace(
                limits, decoder_requirements),
            LzwBlockedHuffmanProfileError::none);
  std::vector<std::byte> decoder_encoded(
      decoder_requirements.frame_encoded_bytes);
  std::vector<std::byte> decoder_staging(
      decoder_requirements.dictionary_staging_bytes);
  std::vector<std::byte> decoder_output(
      decoder_requirements.frame_decoded_bytes);
  std::vector<std::byte> decoder_opaque(decoder_requirements.views_bytes);
  LzwBlockedHuffmanDecoderViews decoder_views{};
  ASSERT_EQ(partition_lzw_blocked_huffman_decoder_views(
                decoder_requirements, decoder_opaque, decoder_views),
            LzwBlockedHuffmanWorkspaceError::none);
  LzwBlockedHuffmanFrameStreamingDecoder decoder{
      limits,         decoder_encoded,      decoder_staging,
      decoder_output, decoder_views.blocks, decoder_views.phrases};
  EXPECT_EQ(decoder.process({}, {}, 0).status,
            marc::core::StreamStatus::need_input);
}

TEST(LzwBlockedHuffmanFrameStreaming, OneByteBuffersMatchFrameOracle) {
  const auto expected = encoded_stream();
  std::array<std::byte, 2> frame_input{};
  std::array<std::byte, 4> staging{};
  std::array<std::byte, 76> frame_encoded{};
  std::array<marc::dictionary::internal::LzwEncoderEntry, 1> entries{};
  LzwBlockedHuffmanFrameStreamingEncoder encoder{
      config(raw.size()), {}, {}, frame_input, staging, frame_encoded, entries};
  std::vector<std::byte> actual;
  std::size_t input_offset{};
  std::array<std::byte, 1> output{};
  marc::core::StreamStatus status{};
  do {
    const auto count = std::min<std::size_t>(1, raw.size() - input_offset);
    const auto chunk =
        std::span<const std::byte>{raw}.subspan(input_offset, count);
    const auto flags =
        input_offset + count == raw.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
    const auto result = encoder.process(chunk, output, flags);
    ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
    ASSERT_NE(result.status, marc::core::StreamStatus::error);
    input_offset += result.input_consumed;
    if (result.output_produced != 0)
      actual.push_back(output[0]);
    status = result.status;
  } while (status != marc::core::StreamStatus::end_of_stream);
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(encoder.process({}, {}, 0).status,
            marc::core::StreamStatus::end_of_stream);
}

TEST(LzwBlockedHuffmanFrameStreaming, DecoderHandlesOneByteBoundaries) {
  const auto encoded = encoded_stream();
  std::array<std::byte, 76> frame_encoded{};
  std::array<std::byte, 4> staging{};
  std::array<std::byte, 2> decoded{};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
  LzwBlockedHuffmanFrameStreamingDecoder decoder{
      {}, frame_encoded, staging, decoded, views, phrases};
  std::vector<std::byte> actual;
  std::size_t input_offset{};
  std::array<std::byte, 1> output{};
  marc::core::StreamStatus status{};
  do {
    const auto count = std::min<std::size_t>(1, encoded.size() - input_offset);
    const auto chunk =
        std::span<const std::byte>{encoded}.subspan(input_offset, count);
    const auto flags =
        input_offset + count == encoded.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
    const auto result = decoder.process(chunk, output, flags);
    ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
    ASSERT_NE(result.status, marc::core::StreamStatus::error);
    input_offset += result.input_consumed;
    if (result.output_produced != 0)
      actual.push_back(output[0]);
    status = result.status;
  } while (status != marc::core::StreamStatus::end_of_stream);
  EXPECT_TRUE(std::ranges::equal(actual, raw));
  EXPECT_EQ(decoder.process({}, {}, 0).status,
            marc::core::StreamStatus::end_of_stream);
}

TEST(LzwBlockedHuffmanFrameStreaming, LaterCorruptionIsStickyAndFrameAtomic) {
  auto encoded = encoded_stream();
  constexpr std::size_t second_frame_payload = 80 + 75 + 56 + 16;
  encoded[second_frame_payload + 2] = std::byte{0xff};
  std::array<std::byte, 76> frame_encoded{};
  std::array<std::byte, 4> staging{};
  std::array<std::byte, 2> decoded{};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
  LzwBlockedHuffmanFrameStreamingDecoder decoder{
      {}, frame_encoded, staging, decoded, views, phrases};
  std::array<std::byte, raw.size()> output{};
  output.fill(std::byte{0x5a});
  const auto result = decoder.process(
      encoded, output,
      marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
  EXPECT_EQ(result.output_produced, 2U);
  EXPECT_EQ(output[0], std::byte{'A'});
  EXPECT_EQ(output[1], std::byte{'B'});
  EXPECT_EQ(output[2], std::byte{0x5a});
  EXPECT_EQ(decoder.process({}, {}, 0).error.code,
            marc::core::ErrorCode::malformed_stream);
}

TEST(LzwBlockedHuffmanFrameStreaming,
     RejectsWorkspaceShortageTruncationAndReset) {
  const auto encoded = encoded_stream();
  std::array<std::byte, 76> frame_encoded{};
  std::array<std::byte, 4> staging{};
  std::array<std::byte, 2> decoded{};
  std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
  std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
  std::array<std::byte, raw.size()> output{};
  const auto end = marc::core::flag_value(marc::core::ProcessFlags::end_input);

  LzwBlockedHuffmanFrameStreamingDecoder short_phrases{
      {},      frame_encoded,
      staging, decoded,
      views,   std::span<marc::dictionary::internal::LzwPhraseEntry>{}};
  EXPECT_EQ(short_phrases.process(encoded, output, end).error.code,
            marc::core::ErrorCode::out_of_memory);

  LzwBlockedHuffmanFrameStreamingDecoder truncated{
      {}, frame_encoded, staging, decoded, views, phrases};
  EXPECT_EQ(truncated
                .process(std::span<const std::byte>{encoded}.first(
                             encoded.size() - 1),
                         output, end)
                .error.code,
            marc::core::ErrorCode::malformed_stream);

  LzwBlockedHuffmanFrameStreamingDecoder reset{{},      frame_encoded, staging,
                                               decoded, views,         phrases};
  EXPECT_EQ(reset
                .process({}, {},
                         marc::core::flag_value(
                             marc::core::ProcessFlags::reset_block))
                .error.code,
            marc::core::ErrorCode::unsupported);
}

TEST(LzwBlockedHuffmanFrameStreaming, EmptyStreamAndPrematureEndAreExact) {
  std::array<std::byte, lzw_blocked_huffman_stream_prefix_size> prefix{};
  std::array<std::byte, 1> unused{};
  LzwBlockedHuffmanFrameStreamingEncoder empty_encoder{
      config(0), {}, {}, {}, {}, unused, {}};
  const auto encoded = empty_encoder.process(
      {}, prefix, marc::core::flag_value(marc::core::ProcessFlags::end_input));
  ASSERT_EQ(encoded.status, marc::core::StreamStatus::end_of_stream);
  EXPECT_EQ(encoded.output_produced, prefix.size());

  LzwBlockedHuffmanFrameStreamingDecoder empty_decoder{{}, {}, {}, {}, {}, {}};
  EXPECT_EQ(
      empty_decoder
          .process(prefix, {},
                   marc::core::flag_value(marc::core::ProcessFlags::end_input))
          .status,
      marc::core::StreamStatus::end_of_stream);

  std::array<std::byte, 2> frame_input{};
  std::array<std::byte, 4> staging{};
  std::array<std::byte, 76> frame_encoded{};
  std::array<marc::dictionary::internal::LzwEncoderEntry, 1> entries{};
  LzwBlockedHuffmanFrameStreamingEncoder premature{
      config(raw.size()), {}, {}, frame_input, staging, frame_encoded, entries};
  EXPECT_EQ(
      premature
          .process(std::span<const std::byte>{raw}.first<1>(), {},
                   marc::core::flag_value(marc::core::ProcessFlags::end_input))
          .error.code,
      marc::core::ErrorCode::invalid_argument);
}

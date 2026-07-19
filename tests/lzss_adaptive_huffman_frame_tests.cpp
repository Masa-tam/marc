#include "frame/lzss_adaptive_huffman_frame.hpp"

#include "core/endian.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzssAdaptiveHuffmanFrameValidationError;

constexpr std::array<std::byte, 75> single_literal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x82}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_a(
    const std::uint32_t raw_size = 1) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size = 1) {
    marc::entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        tokens, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::adaptive_huffman_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(plan.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::adaptive_huffman_descriptor_size;
    const auto stream = stream_for_a(raw_size);
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_adaptive_huffman_descriptor(
                  descriptor, header.dictionary_serialized_size,
                  header.compressed_payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  tokens, limits,
                  std::span<std::byte>{frame}.subspan(
                      marc::frame::frame_header_size
                      + marc::entropy::internal::
                          adaptive_huffman_descriptor_size),
                  descriptor).error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    return frame;
}

} // namespace

TEST(LzssAdaptiveHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, staging);
    ASSERT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, 2U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 3U);
    constexpr std::array expected{std::byte{0x00}, std::byte{0x41}};
    EXPECT_EQ(staging, expected);
}

TEST(LzssAdaptiveHuffmanFrameValidator, RejectsTruncationAndTrailingBytes) {
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        std::array staging{std::byte{0xa5}, std::byte{0xa5}};
        const auto result = marc::frame::validate_lzss_adaptive_huffman_frame(
            stream_for_a(), {}, {}, 0, 0,
            std::span<const std::byte>{single_literal_frame}.first(size),
            staging);
        EXPECT_NE(result.error,
                  LzssAdaptiveHuffmanFrameValidationError::none)
            << size;
    }

    std::vector<std::byte> trailing(single_literal_frame.begin(),
                                    single_literal_frame.end());
    trailing.push_back(std::byte{0x00});
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, trailing, staging).error,
              LzssAdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzssAdaptiveHuffmanFrameValidator, ChecksBoundsBeforeStagingWrites) {
    std::array short_staging{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  short_staging).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0xa5});

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 20;
    limits.max_internal_buffered_bytes = 20;
    std::array staging{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  staging).error,
              LzssAdaptiveHuffmanFrameValidationError::workspace_limit);
    EXPECT_EQ(staging[0], std::byte{0xa5});
    EXPECT_EQ(staging[1], std::byte{0xa5});
}

TEST(LzssAdaptiveHuffmanFrameValidator, RejectsImpossibleDeclaredExtents) {
    auto excessive_dictionary = single_literal_frame;
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_dictionary, 20, 3));
    std::array<std::byte, 3> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, excessive_dictionary,
                  staging).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);

    std::vector<std::byte> excessive_payload(56 + 16 + 67);
    std::ranges::copy(
        std::span<const std::byte>{single_literal_frame}.first<56>(),
                      excessive_payload.begin());
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_payload, 24, 67));
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_payload, 56, 2));
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_payload, 60, 67));
    excessive_payload[64] = std::byte{0x01};
    std::array<std::byte, 2> token_staging{};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, excessive_payload,
                  token_staging).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  invalid_entropy_extent);
}

TEST(LzssAdaptiveHuffmanFrameValidator, RejectsMalformedLayers) {
    auto invalid_descriptor = single_literal_frame;
    invalid_descriptor[71] = std::byte{0x01};
    std::array descriptor_staging{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_descriptor,
                  descriptor_staging).error,
              LzssAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(descriptor_staging[0], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x02}, std::byte{0x41}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens);
    std::array<std::byte, 2> dictionary_staging{};
    const auto result = marc::frame::validate_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, invalid_dictionary,
        dictionary_staging);
    EXPECT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
}

TEST(LzssAdaptiveHuffmanFrameValidator, RejectsPipelineAndSequenceMismatch) {
    auto stream = stream_for_a();
    stream.entropy_block_size = 1;
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, staging).error,
              LzssAdaptiveHuffmanFrameValidationError::unsupported_pipeline);

    const auto sequence = marc::frame::validate_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 1, 0, single_literal_frame, staging);
    EXPECT_EQ(sequence.error,
              LzssAdaptiveHuffmanFrameValidationError::header_error);
    EXPECT_EQ(sequence.header_error,
              marc::frame::FrameHeaderError::unexpected_sequence);
}

TEST(LzssAdaptiveHuffmanFrameEncoder, PlansAndEmitsIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    const auto plan = marc::frame::plan_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(plan.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.raw_size, 1U);
    EXPECT_EQ(plan.dictionary_size, 2U);
    EXPECT_EQ(plan.descriptor_size, 16U);
    EXPECT_EQ(plan.payload_size, 3U);
    EXPECT_EQ(plan.serialized_size, single_literal_frame.size());
    constexpr std::array expected_tokens{
        std::byte{0x00}, std::byte{0x41}};
    EXPECT_EQ(staging, expected_tokens);

    std::array<std::byte, single_literal_frame.size()> encoded{};
    const auto result = marc::frame::encode_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, encoded);
    ASSERT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(encoded, single_literal_frame);
}

TEST(LzssAdaptiveHuffmanFrameEncoder,
     RoundTripsOverlappingMatchDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 12> encode_staging{};
    const auto plan = marc::frame::plan_lzss_adaptive_huffman_frame(
        stream_for_a(raw.size()), {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.dictionary_size, 11U);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_adaptive_huffman_frame(
                  stream_for_a(raw.size()), {}, {}, 0, 0, raw,
                  encode_staging, first).error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzss_adaptive_huffman_frame(
                  stream_for_a(raw.size()), {}, {}, 0, 0, raw,
                  encode_staging, second).error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<std::byte, 12> decode_staging{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(raw.size()), {}, {}, 0, 0, first,
                  decode_staging, raw_staging, decoded).error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssAdaptiveHuffmanFrameEncoder, CapacityFailuresAreOutputAtomic) {
    constexpr std::array raw{std::byte{'A'}};
    std::array short_staging{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::plan_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw,
                  short_staging).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0xa5});

    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_literal_frame.size() - 1> short_output{};
    short_output.fill(std::byte{0xa5});
    const auto result = marc::frame::encode_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, short_output);
    EXPECT_EQ(result.error, LzssAdaptiveHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzssAdaptiveHuffmanFrameEncoder, RejectsInvalidRawFrameExtent) {
    std::array<std::byte, 4> staging{};
    EXPECT_EQ(marc::frame::plan_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0,
                  std::span<const std::byte>{}, staging).error,
              LzssAdaptiveHuffmanFrameValidationError::input_size_mismatch);
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw, staging).error,
              LzssAdaptiveHuffmanFrameValidationError::input_size_mismatch);
}

TEST(LzssAdaptiveHuffmanFrameEncoder, EnforcesAggregateWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 20;
    limits.max_internal_buffered_bytes = 20;
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::plan_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, raw, staging).error,
              LzssAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzssAdaptiveHuffmanFrameDecoder, PublishesHandVectorAfterPrivateDecode) {
    std::array<std::byte, 2> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    auto result =
        marc::frame::decode_lzss_adaptive_huffman_frame_to_staging(
            stream_for_a(), {}, {}, 0, 0, single_literal_frame,
            dictionary_staging, raw_staging);
    ASSERT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});

    raw_staging[0] = std::byte{0xa5};
    result = marc::frame::decode_lzss_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame,
        dictionary_staging, raw_staging, output);
    ASSERT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xa5});
}

TEST(LzssAdaptiveHuffmanFrameDecoder, ReconstructsOverlappingMatch) {
    std::array<std::byte, 11> tokens{};
    std::size_t written{};
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::literal,
                   0, 0, 'A'},
                  tokens, written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(written, 2U);
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::match,
                   1, 5, 0},
                  std::span<std::byte>{tokens}.subspan(written), written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(written, 9U);
    const auto frame = frame_for_tokens(tokens, 6);
    std::array<std::byte, 11> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    std::array<std::byte, 6> output{};
    const auto result = marc::frame::decode_lzss_adaptive_huffman_frame(
        stream_for_a(6), {}, {}, 0, 0, frame, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error,
              LzssAdaptiveHuffmanFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssAdaptiveHuffmanFrameDecoder, CapacityFailuresPrecedeMutation) {
    std::array dictionary_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  dictionary_staging, std::span<std::byte>{}, output).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  raw_staging_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});

    EXPECT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  dictionary_staging, raw_staging,
                  std::span<std::byte>{}).error,
              LzssAdaptiveHuffmanFrameValidationError::raw_output_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzssAdaptiveHuffmanFrameDecoder, IncludesRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 21;
    limits.max_internal_buffered_bytes = 21;
    std::array dictionary_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  dictionary_staging, raw_staging, output).error,
              LzssAdaptiveHuffmanFrameValidationError::workspace_limit);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});
}

TEST(LzssAdaptiveHuffmanFrameDecoder, MalformedLayersNeverPublishRawBytes) {
    std::array<std::byte, 2> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}};
    std::array output{std::byte{0xa5}};
    auto malformed_descriptor = single_literal_frame;
    malformed_descriptor[71] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, malformed_descriptor,
                  dictionary_staging, raw_staging, output).error,
              LzssAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x02}, std::byte{0x41}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens);
    EXPECT_EQ(marc::frame::decode_lzss_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_dictionary,
                  dictionary_staging, raw_staging, output).error,
              LzssAdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(output[0], std::byte{0xa5});
}

#include "frame/lz77_adaptive_huffman_frame.hpp"

#include "core/endian.hpp"
#include "dictionary/lz77_format.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz77AdaptiveHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    return stream_for(1);
}

constexpr std::array<std::byte, 76> single_literal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xff}, std::byte{0x17}, std::byte{0x74}};

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
    const auto stream = stream_for(raw_size);
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

TEST(Lz77AdaptiveHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, staging);
    ASSERT_EQ(result.error,
              Lz77AdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, 16U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 4U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[12], std::byte{0x41});
}

TEST(Lz77AdaptiveHuffmanFrameValidator, StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 16> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz77_adaptive_huffman_frame(
                      stream_for_a(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_literal_frame}.first(
                          size),
                      staging).error,
                  Lz77AdaptiveHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsShortStagingBeforeMutation) {
    std::array<std::byte, 15> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77AdaptiveHuffmanFrameValidator, EnforcesAggregateWorkspaceBeforeMutation) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 16 + 4 + 16 - 1;
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsMalformedDescriptorBeforeMutation) {
    auto malformed = single_literal_frame;
    malformed[71] = std::byte{1};
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error,
              Lz77AdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsNonzeroPayloadPadding) {
    auto malformed = single_literal_frame;
    malformed.back() |= std::byte{0x80};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error,
              Lz77AdaptiveHuffmanFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::AdaptiveHuffmanDecodeError::
                  nonzero_padding);
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsEntropyDecodedInvalidLz77Token) {
    std::array<std::byte, 16> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed = frame_for_tokens(invalid_tokens);
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error, Lz77AdaptiveHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    std::vector<std::byte> malformed(56 + 16 + 529);
    std::ranges::copy(
        std::span<const std::byte>{single_literal_frame}.first<72>(),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{529}));
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, malformed, staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::
                  invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77AdaptiveHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lz77_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, staging).error,
              Lz77AdaptiveHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(Lz77AdaptiveHuffmanFrameDecoder, PublishesHandVectorOnlyAfterPrivateDecode) {
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 3> raw_staging{};
    std::array<std::byte, 3> output{};
    raw_staging.fill(std::byte{0x5a});
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame,
        dictionary_staging, raw_staging, output);
    ASSERT_EQ(result.error,
              Lz77AdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(Lz77AdaptiveHuffmanFrameDecoder, ReconstructsOverlappingMatch) {
    std::array<std::byte, 32> tokens{};
    const marc::dictionary::internal::Lz77Token literal{
        marc::dictionary::internal::Lz77TokenTag::literal, 0, 0, 'A'};
    const marc::dictionary::internal::Lz77Token match{
        marc::dictionary::internal::Lz77TokenTag::terminal_match, 1, 4, 0};
    ASSERT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  literal,
                  std::span<std::byte,
                            marc::dictionary::internal::lz77_token_size>{
                      tokens.data(),
                      marc::dictionary::internal::lz77_token_size}),
              marc::dictionary::internal::Lz77FormatError::none);
    ASSERT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  match,
                  std::span<std::byte,
                            marc::dictionary::internal::lz77_token_size>{
                      tokens.data()
                          + marc::dictionary::internal::lz77_token_size,
                      marc::dictionary::internal::lz77_token_size}),
              marc::dictionary::internal::Lz77FormatError::none);
    const auto encoded = frame_for_tokens(tokens, 5);
    std::array<std::byte, 32> dictionary_staging{};
    std::array<std::byte, 5> raw_staging{};
    std::array<std::byte, 5> output{};
    const auto result = marc::frame::decode_lz77_adaptive_huffman_frame(
        stream_for(5), {}, {}, 0, 0, encoded, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error,
              Lz77AdaptiveHuffmanFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(Lz77AdaptiveHuffmanFrameDecoder, CapacityFailuresPrecedePrivateMutation) {
    std::array<std::byte, 16> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  dictionary_staging, std::span<std::byte>{}, output).error,
              Lz77AdaptiveHuffmanFrameValidationError::
                  raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(output[0], std::byte{0x5a});

    EXPECT_EQ(marc::frame::decode_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  dictionary_staging, raw_staging,
                  std::span<std::byte>{}).error,
              Lz77AdaptiveHuffmanFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

TEST(Lz77AdaptiveHuffmanFrameDecoder, IncludesRawStagingInAggregateLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 16 + 4 + 16 + 1 - 1;
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    dictionary_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  dictionary_staging, raw_staging, output).error,
              Lz77AdaptiveHuffmanFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(Lz77AdaptiveHuffmanFrameDecoder, MalformedLayersNeverPublishRawBytes) {
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto invalid_descriptor = single_literal_frame;
    invalid_descriptor[71] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_descriptor,
                  dictionary_staging, raw_staging, output).error,
              Lz77AdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(output[0], std::byte{0x5a});

    std::array<std::byte, 16> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens);
    EXPECT_EQ(marc::frame::decode_lz77_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_dictionary,
                  dictionary_staging, raw_staging, output).error,
              Lz77AdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});
}

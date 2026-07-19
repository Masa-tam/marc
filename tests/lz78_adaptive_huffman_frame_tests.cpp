#include "frame/lz78_adaptive_huffman_frame.hpp"

#include "dictionary/lz78_encoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz78AdaptiveHuffmanFrameValidationError;

constexpr std::array<std::byte, 8> pair_a{
    std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

constexpr std::array<std::byte, 75> single_pair_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x82}, std::byte{0x7e}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    return stream_for_size(1);
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
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream_for_size(raw_size), limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_adaptive_huffman_descriptor(
                  descriptor, tokens.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  tokens, {},
                  std::span<std::byte>{frame}.subspan(
                      marc::frame::frame_header_size
                      + marc::entropy::internal::
                          adaptive_huffman_descriptor_size),
                  descriptor).error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    return frame;
}

[[nodiscard]] std::vector<std::byte> frame_for_raw(
    const std::span<const std::byte> raw) {
    std::vector<marc::dictionary::internal::Lz78EncoderEntry> workspace(
        marc::dictionary::internal::lz78_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lz78_token_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::Lz78EncodeError::none);
    std::vector<std::byte> tokens(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lz78_token_stream(
                  raw, {}, {}, workspace, tokens).error,
              marc::dictionary::internal::Lz78EncodeError::none);
    return frame_for_tokens(
        tokens, static_cast<std::uint32_t>(raw.size()));
}

TEST(Lz78AdaptiveHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_pair_frame, staging, phrases);
    ASSERT_EQ(result.error,
              Lz78AdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_pair_frame.size());
    EXPECT_EQ(result.dictionary_size, pair_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.phrase_entries, 1U);
    EXPECT_EQ(staging, pair_a);
    EXPECT_EQ(phrases[0].prefix_index, 0U);
    EXPECT_EQ(phrases[0].symbol, static_cast<std::uint8_t>('A'));
    EXPECT_EQ(phrases[0].length, 1U);
}

TEST(Lz78AdaptiveHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    for (std::size_t size = 0; size < single_pair_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz78_adaptive_huffman_frame(
                      stream_for_a(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_pair_frame}.first(size),
                      staging, phrases).error,
                  Lz78AdaptiveHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(
        single_pair_frame.begin(), single_pair_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, staging, phrases)
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(Lz78AdaptiveHuffmanFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_pair_frame,
                  std::span<std::byte>{}, phrases).error,
              Lz78AdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_pair_frame, staging,
                  std::span<marc::dictionary::internal::Lz78PhraseEntry>{})
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz78AdaptiveHuffmanFrameValidator, CountsAlignedPhraseWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required =
        16 + 3 + 8 + sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_pair_frame,
                  staging, phrases).error,
              Lz78AdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(Lz78AdaptiveHuffmanFrameValidator,
     RejectsDescriptorAndPaddingBeforePhraseValidation) {
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};

    auto descriptor = single_pair_frame;
    descriptor[64] = std::byte{};
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, descriptor, staging, phrases)
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto padding = single_pair_frame;
    padding.back() |= std::byte{0x80};
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, padding, staging, phrases)
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::entropy_decode_error);
}

TEST(Lz78AdaptiveHuffmanFrameValidator, RejectsInvalidPhraseReference) {
    auto invalid_tokens = pair_a;
    invalid_tokens[4] = std::byte{1};
    const auto malformed = frame_for_tokens(invalid_tokens);
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging, phrases);
    EXPECT_EQ(result.error, Lz78AdaptiveHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz78ValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::Lz78FormatError::
                  invalid_phrase_index);
}

TEST(Lz78AdaptiveHuffmanFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    auto extent = single_pair_frame;
    extent[20] = std::byte{7};
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extent, staging, phrases)
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_pair_frame, staging,
                  phrases).error,
              Lz78AdaptiveHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lz78_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, single_pair_frame, staging, phrases)
                  .error,
              Lz78AdaptiveHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(Lz78AdaptiveHuffmanFrameDecoder,
     PublishesHandVectorOnlyAfterPrivateReconstruction) {
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array<std::byte, 3> raw_staging{};
    raw_staging.fill(std::byte{0x5a});
    std::array<std::byte, 3> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz78_adaptive_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_pair_frame, staging, phrases,
        raw_staging, output);
    ASSERT_EQ(result.error,
              Lz78AdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(Lz78AdaptiveHuffmanFrameDecoder, ReconstructsNestedPhraseGraph) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto frame = frame_for_raw(raw);
    std::vector<std::byte> staging(raw.size() * 8);
    std::vector<marc::dictionary::internal::Lz78PhraseEntry> phrases(
        raw.size());
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> output{};
    const auto result = marc::frame::decode_lz78_adaptive_huffman_frame(
        stream_for_size(static_cast<std::uint32_t>(raw.size())), {}, {}, 0, 0,
        frame, staging, phrases, raw_staging, output);
    ASSERT_EQ(result.error,
              Lz78AdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(output, raw);
    EXPECT_EQ(raw_staging, raw);
}

TEST(Lz78AdaptiveHuffmanFrameDecoder,
     CapacityFailuresPrecedePrivateMutation) {
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::decode_lz78_adaptive_huffman_frame_to_staging(
                  stream_for_a(), {}, {}, 0, 0, single_pair_frame, staging,
                  phrases, std::span<std::byte>{}).error,
              Lz78AdaptiveHuffmanFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    EXPECT_EQ(marc::frame::decode_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_pair_frame, staging,
                  phrases, raw_staging, std::span<std::byte>{}).error,
              Lz78AdaptiveHuffmanFrameValidationError::raw_output_too_small);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz78AdaptiveHuffmanFrameDecoder, CountsRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 3 + 8 + 1
        + sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array<std::byte, 1> raw_staging{};
    EXPECT_EQ(marc::frame::decode_lz78_adaptive_huffman_frame_to_staging(
                  stream_for_a(), {}, limits, 0, 0, single_pair_frame,
                  staging, phrases, raw_staging).error,
              Lz78AdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(Lz78AdaptiveHuffmanFrameDecoder,
     MalformedLayersNeverPublishRawBytes) {
    std::array<std::byte, 8> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    std::array<std::byte, 1> output{std::byte{0x5a}};

    auto descriptor = single_pair_frame;
    descriptor[64] = std::byte{};
    EXPECT_EQ(marc::frame::decode_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, descriptor, staging, phrases,
                  raw_staging, output).error,
              Lz78AdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto invalid_tokens = pair_a;
    invalid_tokens[4] = std::byte{1};
    const auto invalid_phrase = frame_for_tokens(invalid_tokens);
    EXPECT_EQ(marc::frame::decode_lz78_adaptive_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_phrase, staging,
                  phrases, raw_staging, output).error,
              Lz78AdaptiveHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
    EXPECT_EQ(output[0], std::byte{0x5a});
}

} // namespace

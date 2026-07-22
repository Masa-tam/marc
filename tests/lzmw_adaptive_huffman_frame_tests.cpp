#include "frame/lzmw_adaptive_huffman_frame.hpp"

#include "dictionary/lzmw_encoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzmwAdaptiveHuffmanFrameValidationError;

constexpr std::array reference_a{
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 75> single_reference_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x41}, std::byte{0x00}, std::byte{0x0c}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_references(
    const std::span<const std::byte> references,
    const std::uint32_t raw_size) {
    marc::entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        references, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::adaptive_huffman_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(references.size());
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
                  descriptor, references.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  references, {},
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
    std::vector<marc::dictionary::internal::LzmwEncoderEntry> workspace(
        marc::dictionary::internal::lzmw_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lzmw_token_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::LzmwEncodeError::none);
    std::vector<std::byte> references(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lzmw_token_stream(
                  raw, {}, {}, workspace, references).error,
              marc::dictionary::internal::LzmwEncodeError::none);
    return frame_for_references(
        references, static_cast<std::uint32_t>(raw.size()));
}

TEST(LzmwAdaptiveHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, single_reference_frame, staging, {});
    ASSERT_EQ(result.error,
              LzmwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_reference_frame.size());
    EXPECT_EQ(result.dictionary_size, reference_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(staging, reference_a);
}

TEST(LzmwAdaptiveHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, reference_a.size()> staging{};
    for (std::size_t size = 0; size < single_reference_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzmw_adaptive_huffman_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_reference_frame}.first(
                          size),
                      staging, {}).error,
                  LzmwAdaptiveHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_reference_frame.begin(),
                                    single_reference_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzmwAdaptiveHuffmanFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, reference_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, single_reference_frame,
                  {}, {}).error,
              LzmwAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    const auto pair = marc::frame::validate_lzmw_adaptive_huffman_frame(
        stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, phrases);
    ASSERT_EQ(pair.error, LzmwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(pair.token_count, 2U);
    EXPECT_EQ(pair.phrase_entries, 1U);
    EXPECT_EQ(pair.dictionary_entries, 1U);
    EXPECT_EQ(pair.expansion_entries, 2U);
    EXPECT_EQ(phrases[0].left_reference, 0x41U);
    EXPECT_EQ(phrases[0].right_reference, 0x42U);
    EXPECT_EQ(phrases[0].length, 2U);
    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwAdaptiveHuffmanFrameValidator, CountsAllValidationWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 3 + 4;
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, reference_a.size()> staging{};
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, limits, 0, 0,
                  single_reference_frame, staging, {}).error,
              LzmwAdaptiveHuffmanFrameValidationError::workspace_limit);

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 8> pair_staging{};
    pair_staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    const std::uint64_t pair_required =
        16 + frame.size() - marc::frame::frame_header_size - 16
        + pair_staging.size()
        + sizeof(marc::dictionary::internal::LzmwPhraseEntry);
    limits.max_internal_buffered_bytes = pair_required - 1;
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(2), {}, limits, 0, 0, frame, pair_staging,
                  phrases).error,
              LzmwAdaptiveHuffmanFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzmwAdaptiveHuffmanFrameValidator,
     RejectsDescriptorAndEntropyPaddingBeforeLzmwValidation) {
    std::array<std::byte, reference_a.size()> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = single_reference_frame;
    descriptor[64] = std::byte{};
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto padding = single_reference_frame;
    padding.back() |= std::byte{0x80};
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, padding, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::entropy_decode_error);
}

TEST(LzmwAdaptiveHuffmanFrameValidator,
     RejectsInvalidReferenceAfterEntropyDecode) {
    constexpr std::array forward_reference{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    const auto forward = frame_for_references(forward_reference, 1);
    std::array<std::byte, reference_a.size()> staging{};
    const auto result = marc::frame::validate_lzmw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, forward, staging, {});
    EXPECT_EQ(result.error, LzmwAdaptiveHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzmwValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzmwFormatError::
                  invalid_phrase_reference);

    const auto wrong_size = frame_for_references(reference_a, 2);
    const auto size_result =
        marc::frame::validate_lzmw_adaptive_huffman_frame(
            stream_for_size(2), {}, {}, 0, 0, wrong_size, staging, {});
    EXPECT_EQ(size_result.error, LzmwAdaptiveHuffmanFrameValidationError::
                                     dictionary_validation_error);
    EXPECT_EQ(size_result.dictionary_error,
              marc::dictionary::internal::LzmwValidationError::premature_end);
}

TEST(LzmwAdaptiveHuffmanFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, reference_a.size()> staging{};
    auto extent = single_reference_frame;
    extent[20] = std::byte{0x08};
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    extent = single_reference_frame;
    extent[20] = std::byte{0x03};
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 1, 0, single_reference_frame,
                  staging, {}).error,
              LzmwAdaptiveHuffmanFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzmw_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, single_reference_frame, staging, {})
                  .error,
              LzmwAdaptiveHuffmanFrameValidationError::unsupported_pipeline);
}

} // namespace

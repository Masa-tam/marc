#include "frame/lzw_adaptive_huffman_frame.hpp"

#include "dictionary/lzw_encoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::LzwAdaptiveHuffmanFrameValidationError;

constexpr std::array packed_code_a{std::byte{0x41}, std::byte{0x00}};
constexpr std::array<std::byte, 75> single_code_frame{
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
    std::byte{0x41}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_size(
    const std::uint32_t size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_codes(
    const std::span<const std::byte> codes,
    const std::uint32_t raw_size) {
    marc::entropy::internal::AdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_adaptive_huffman_frame(
        codes, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::AdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::adaptive_huffman_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(codes.size());
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
                  descriptor, codes.size(), plan.payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                adaptive_huffman_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          adaptive_huffman_descriptor_size}),
              marc::entropy::internal::AdaptiveHuffmanFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_adaptive_huffman_frame(
                  codes, {},
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
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    const auto plan = marc::dictionary::internal::plan_lzw_code_stream(
        raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, marc::dictionary::internal::LzwEncodeError::none);
    std::vector<std::byte> codes(plan.output_size);
    EXPECT_EQ(marc::dictionary::internal::encode_lzw_code_stream(
                  raw, {}, {}, workspace, codes).error,
              marc::dictionary::internal::LzwEncodeError::none);
    return frame_for_codes(codes, static_cast<std::uint32_t>(raw.size()));
}

TEST(LzwAdaptiveHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, single_code_frame, staging, {});
    ASSERT_EQ(result.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_code_frame.size());
    EXPECT_EQ(result.dictionary_size, packed_code_a.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwAdaptiveHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 2> staging{};
    for (std::size_t size = 0; size < single_code_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzw_adaptive_huffman_frame(
                      stream_for_size(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_code_frame}.first(size),
                      staging, {}).error,
                  LzwAdaptiveHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_code_frame.begin(),
                                    single_code_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extended, staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzwAdaptiveHuffmanFrameValidator,
     RejectsWorkspaceShortageBeforeEntropyOutput) {
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, single_code_frame, {}, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 3> pair_staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
    ASSERT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging,
                  phrases).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, pair_staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::
                  phrase_workspace_too_small);
}

TEST(LzwAdaptiveHuffmanFrameValidator, CountsAlignedPhraseWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = 16 + 3 + 2;
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, limits, 0, 0, single_code_frame,
                  staging, {}).error,
              LzwAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzwAdaptiveHuffmanFrameValidator,
     RejectsDescriptorAndEntropyPaddingBeforeLzwValidation) {
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    auto descriptor = single_code_frame;
    descriptor[64] = std::byte{};
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, descriptor, staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    auto padding = single_code_frame;
    padding.back() |= std::byte{0x80};
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, padding, staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::entropy_decode_error);
}

TEST(LzwAdaptiveHuffmanFrameValidator,
     RejectsNonzeroLzwPaddingAfterEntropyDecode) {
    constexpr std::array malformed_codes{
        std::byte{0x41}, std::byte{0x80}};
    const auto malformed = frame_for_codes(malformed_codes, 1);
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, malformed, staging, {});
    EXPECT_EQ(result.error, LzwAdaptiveHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzwValidationError::code_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzwFormatError::nonzero_padding);
}

TEST(LzwAdaptiveHuffmanFrameValidator,
     RejectsInvalidExtentSequenceAndPipeline) {
    std::array<std::byte, 2> staging{};
    auto extent = single_code_frame;
    extent[20] = std::byte{0x03};
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, extent, staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::
                  invalid_dictionary_extent);
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 1, 0, single_code_frame,
                  staging, {}).error,
              LzwAdaptiveHuffmanFrameValidationError::header_error);
    auto stream = stream_for_size(1);
    stream.entropy_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzw_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, single_code_frame, staging, {})
                  .error,
              LzwAdaptiveHuffmanFrameValidationError::unsupported_pipeline);
}

} // namespace

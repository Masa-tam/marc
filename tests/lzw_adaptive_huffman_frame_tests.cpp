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

TEST(LzwAdaptiveHuffmanFrameEncoder, PlansExactHandVectorExtent) {
    constexpr std::array raw{std::byte{'A'}};
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::plan_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, raw, workspace, staging);
    ASSERT_EQ(result.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, 2U);
    EXPECT_EQ(result.encoder_entries, 0U);
    EXPECT_EQ(result.code_count, 1U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(result.serialized_size, single_code_frame.size());
    EXPECT_EQ(staging, packed_code_a);
}

TEST(LzwAdaptiveHuffmanFrameEncoder, EmitsExactIndependentHandVector) {
    constexpr std::array raw{std::byte{'A'}};
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_code_frame.size()> output{};
    const auto result = marc::frame::encode_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, raw, workspace, staging, output);
    ASSERT_EQ(result.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(output, single_code_frame);
}

TEST(LzwAdaptiveHuffmanFrameEncoder,
     RoundTripsMultipleCodesDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    std::vector<marc::dictionary::internal::LzwEncoderEntry> workspace(
        marc::dictionary::internal::lzw_encoder_workspace_entries(
            raw.size(), {}));
    std::array<std::byte, raw.size() * 2> encode_staging{};
    const auto stream = stream_for_size(raw.size());
    const auto plan = marc::frame::plan_lzw_adaptive_huffman_frame(
        stream, {}, {}, 0, 0, raw, workspace, encode_staging);
    ASSERT_EQ(plan.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzw_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  first).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzw_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, raw, workspace, encode_staging,
                  second).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::vector<std::byte> decode_staging(plan.dictionary_size);
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame(
                  stream, {}, {}, 0, 0, first, decode_staging, phrases,
                  raw_staging, decoded).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzwAdaptiveHuffmanFrameEncoder,
     CapacityFailuresAreSerializedOutputAtomic) {
    constexpr std::array raw_ab{std::byte{'A'}, std::byte{'B'}};
    std::array<std::byte, 3> pair_staging{};
    pair_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lzw_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, raw_ab, {},
                  pair_staging).error,
              LzwAdaptiveHuffmanFrameValidationError::
                  encoder_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        pair_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));

    constexpr std::array raw_a{std::byte{'A'}};
    std::array<std::byte, 1> short_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::plan_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, raw_a, {},
                  short_staging).error,
              LzwAdaptiveHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0x5a});

    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_code_frame.size() - 1> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, raw_a, {}, staging, short_output);
    EXPECT_EQ(result.error, LzwAdaptiveHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, single_code_frame.size());
    EXPECT_TRUE(std::ranges::all_of(
        short_output, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(LzwAdaptiveHuffmanFrameEncoder,
     RejectsEmptyAndUnexpectedFrameExtent) {
    std::array<marc::dictionary::internal::LzwEncoderEntry, 1> workspace{};
    std::array<std::byte, 4> staging{};
    EXPECT_EQ(marc::frame::plan_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0,
                  std::span<const std::byte>{}, workspace, staging).error,
              LzwAdaptiveHuffmanFrameValidationError::input_size_mismatch);
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, raw, workspace,
                  staging).error,
              LzwAdaptiveHuffmanFrameValidationError::input_size_mismatch);
}

TEST(LzwAdaptiveHuffmanFrameEncoder, EnforcesAggregateWorkspaceBound) {
    constexpr std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required = staging.size()
        + marc::entropy::internal::adaptive_huffman_descriptor_size + 3;
    limits.max_internal_buffered_bytes = required - 1;
    EXPECT_EQ(marc::frame::plan_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, limits, 0, 0, raw, {},
                  staging).error,
              LzwAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzwAdaptiveHuffmanFrameDecoder, ReconstructsHandVectorPrivately) {
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> raw_staging{};
    const auto result =
        marc::frame::decode_lzw_adaptive_huffman_frame_to_staging(
            stream_for_size(1), {}, {}, 0, 0, single_code_frame, staging, {},
            raw_staging);
    ASSERT_EQ(result.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(result.dictionary_decode_error,
              marc::dictionary::internal::LzwDecodeError::none);
    EXPECT_EQ(staging, packed_code_a);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
}

TEST(LzwAdaptiveHuffmanFrameDecoder,
     RejectsSmallRawStagingBeforeEntropyOutput) {
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, single_code_frame,
                  staging, {}, {}).error,
              LzwAdaptiveHuffmanFrameValidationError::
                  raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
}

TEST(LzwAdaptiveHuffmanFrameDecoder, CountsRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t validation_bytes = 16 + 3 + 2;
    limits.max_internal_buffered_bytes = validation_bytes;
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> raw_staging{};
    EXPECT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, limits, 0, 0, single_code_frame,
                  staging, {}, raw_staging).error,
              LzwAdaptiveHuffmanFrameValidationError::workspace_limit);
}

TEST(LzwAdaptiveHuffmanFrameDecoder,
     RoundTripsMultipleCodesAndKeepsMalformedRawStagingUntouched) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 2> staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    ASSERT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame_to_staging(
                  stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
                  phrases, raw_staging).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging, raw);

    auto malformed = single_code_frame;
    malformed[64] = std::byte{};
    std::array<std::byte, 1> guarded_raw{std::byte{0x6b}};
    EXPECT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame_to_staging(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  guarded_raw).error,
              LzwAdaptiveHuffmanFrameValidationError::descriptor_error);
    EXPECT_EQ(guarded_raw[0], std::byte{0x6b});
}

TEST(LzwAdaptiveHuffmanFrameDecoder, PublishesHandVectorAfterSuccess) {
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> raw_staging{};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    const auto result = marc::frame::decode_lzw_adaptive_huffman_frame(
        stream_for_size(1), {}, {}, 0, 0, single_code_frame, staging, {},
        raw_staging, output);
    ASSERT_EQ(result.error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzwAdaptiveHuffmanFrameDecoder,
     RejectsSmallOutputBeforeMutatingAnyStaging) {
    constexpr std::array raw{std::byte{'A'}, std::byte{'B'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, 3> staging{};
    staging.fill(std::byte{0x5a});
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> phrases{};
    std::array<std::byte, 2> raw_staging{
        std::byte{0x6b}, std::byte{0x6b}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame(
                  stream_for_size(2), {}, {}, 0, 0, frame, staging, phrases,
                  raw_staging, output).error,
              LzwAdaptiveHuffmanFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{0x6b};
        }));
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzwAdaptiveHuffmanFrameDecoder, MalformedFrameLeavesOutputUnchanged) {
    auto malformed = single_code_frame;
    malformed.back() |= std::byte{0x80};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x6b}};
    std::array<std::byte, 1> output{std::byte{0x7c}};
    EXPECT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame(
                  stream_for_size(1), {}, {}, 0, 0, malformed, staging, {},
                  raw_staging, output).error,
              LzwAdaptiveHuffmanFrameValidationError::entropy_decode_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x6b});
    EXPECT_EQ(output[0], std::byte{0x7c});
}

TEST(LzwAdaptiveHuffmanFrameDecoder, PublishesCompleteMultipleCodeFrame) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}};
    const auto frame = frame_for_raw(raw);
    std::array<std::byte, raw.size() * 2> staging{};
    std::array<marc::dictionary::internal::LzwPhraseEntry, raw.size()> phrases{};
    std::array<std::byte, raw.size()> raw_staging{};
    std::array<std::byte, raw.size()> output{};
    ASSERT_EQ(marc::frame::decode_lzw_adaptive_huffman_frame(
                  stream_for_size(raw.size()), {}, {}, 0, 0, frame, staging,
                  phrases, raw_staging, output).error,
              LzwAdaptiveHuffmanFrameValidationError::none);
    EXPECT_EQ(raw_staging, raw);
    EXPECT_EQ(output, raw);
}

} // namespace

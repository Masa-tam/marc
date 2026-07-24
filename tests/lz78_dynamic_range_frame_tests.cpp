#include "frame/lz78_dynamic_range_frame.hpp"

#include "core/endian.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz78DynamicRangeFrameValidationError;

constexpr std::array<std::byte, 8> pair_a{
    std::byte{0x00}, std::byte{0x41}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

constexpr std::array<std::byte, 83> single_pair_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0b}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0b}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0xbe},
    std::byte{0x41}, std::byte{0x7c}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = raw_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size) {
    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        tokens, {}, descriptor);
    EXPECT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    std::vector<std::byte> frame(
        marc::frame::frame_header_size
        + marc::entropy::internal::dynamic_range_descriptor_size
        + plan.payload_size);

    marc::frame::FrameHeader header{};
    header.uncompressed_size = raw_size;
    header.dictionary_serialized_size =
        static_cast<std::uint32_t>(tokens.size());
    header.compressed_payload_size =
        static_cast<std::uint32_t>(plan.payload_size);
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::dynamic_range_descriptor_size;
    const auto stream = stream_for(raw_size);
    const marc::core::DecoderLimits limits{};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);
    EXPECT_EQ(marc::entropy::internal::serialize_dynamic_range_descriptor(
                  descriptor, header.dictionary_serialized_size,
                  header.compressed_payload_size, limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    EXPECT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  tokens, limits,
                  std::span<std::byte>{frame}.subspan(
                      marc::frame::frame_header_size
                      + marc::entropy::internal::
                          dynamic_range_descriptor_size),
                  descriptor).error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    return frame;
}

} // namespace

TEST(Lz78DynamicRangeFrameValidator, AcceptsHandVectorIntoPrivateWorkspaces) {
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, single_pair_frame, staging, phrases);
    ASSERT_EQ(result.error, Lz78DynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_pair_frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, pair_a.size());
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 11U);
    EXPECT_EQ(result.phrase_entries, 1U);
    EXPECT_EQ(staging, pair_a);
    EXPECT_EQ(phrases[0].prefix_index, 0U);
    EXPECT_EQ(phrases[0].symbol, static_cast<std::uint8_t>('A'));
    EXPECT_EQ(phrases[0].length, 1U);
}

TEST(Lz78DynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    for (std::size_t size = 0; size < single_pair_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz78_dynamic_range_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_pair_frame}.first(
                          size),
                      staging, phrases).error,
                  Lz78DynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(
        single_pair_frame.begin(), single_pair_frame.end());
    extended.push_back(std::byte{});
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, extended, staging, phrases)
                  .error,
              Lz78DynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(Lz78DynamicRangeFrameValidator,
     ChecksAllWorkspacesBeforeEntropyOutput) {
    std::array short_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, single_pair_frame,
                  short_staging, phrases).error,
              Lz78DynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));

    std::array staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, single_pair_frame, staging,
                  std::span<marc::dictionary::internal::Lz78PhraseEntry>{})
                  .error,
              Lz78DynamicRangeFrameValidationError::
                  phrase_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(Lz78DynamicRangeFrameValidator, CountsAlignedPhraseWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    const std::uint64_t required =
        16 + 11 + 8 + sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, limits, 0, 0, single_pair_frame,
                  staging, phrases).error,
              Lz78DynamicRangeFrameValidationError::workspace_limit);
}

TEST(Lz78DynamicRangeFrameValidator, RejectsImpossibleDeclaredExtents) {
    auto invalid_dictionary = single_pair_frame;
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        invalid_dictionary, 20, 9));
    std::array<std::byte, 9> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, invalid_dictionary, staging,
                  phrases).error,
              Lz78DynamicRangeFrameValidationError::
                  invalid_dictionary_extent);

    std::vector<std::byte> excessive_payload(56 + 16 + 22);
    std::ranges::copy(
        std::span<const std::byte>{single_pair_frame}.first<72>(),
        excessive_payload.begin());
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_payload, 24, 22));
    std::array<std::byte, pair_a.size()> token_staging{};
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, excessive_payload,
                  token_staging, phrases).error,
              Lz78DynamicRangeFrameValidationError::invalid_entropy_extent);
}

TEST(Lz78DynamicRangeFrameValidator,
     RejectsMalformedDescriptorBeforeMutation) {
    auto malformed = single_pair_frame;
    malformed[71] = std::byte{1};
    std::array staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, malformed, staging, phrases);
    EXPECT_EQ(result.error,
              Lz78DynamicRangeFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(
        staging, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
}

TEST(Lz78DynamicRangeFrameValidator,
     ReportsStableInvalidPhraseReferencePosition) {
    auto invalid_tokens = pair_a;
    invalid_tokens[4] = std::byte{1};
    const auto malformed = frame_for_tokens(invalid_tokens, 1);
    std::array<std::byte, invalid_tokens.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    const auto result = marc::frame::validate_lz78_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, malformed, staging, phrases);
    EXPECT_EQ(result.error, Lz78DynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz78ValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::Lz78FormatError::
                  invalid_phrase_index);
    EXPECT_EQ(result.dictionary_token_index, 0U);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
    EXPECT_EQ(staging, invalid_tokens);
}

TEST(Lz78DynamicRangeFrameValidator, RejectsPipelineSequenceAndFrameCap) {
    std::array<std::byte, pair_a.size()> staging{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 1> phrases{};
    auto stream = stream_for(1);
    stream.entropy_block_size = 1;
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_pair_frame, staging, phrases)
                  .error,
              Lz78DynamicRangeFrameValidationError::unsupported_pipeline);

    const auto sequence = marc::frame::validate_lz78_dynamic_range_frame(
        stream_for(1), {}, {}, 1, 0, single_pair_frame, staging, phrases);
    EXPECT_EQ(sequence.error,
              Lz78DynamicRangeFrameValidationError::header_error);
    EXPECT_EQ(sequence.header_error,
              marc::frame::FrameHeaderError::unexpected_sequence);

    stream = stream_for((UINT32_C(1) << 21) + 1);
    EXPECT_EQ(marc::frame::validate_lz78_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_pair_frame, staging, phrases)
                  .error,
              Lz78DynamicRangeFrameValidationError::unsupported_pipeline);
}

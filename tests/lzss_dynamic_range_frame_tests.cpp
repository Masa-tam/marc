#include "frame/lzss_dynamic_range_frame.hpp"

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

using marc::frame::LzssDynamicRangeFrameValidationError;

constexpr std::array<std::byte, 79> single_literal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0xbe},
    std::byte{0x41}, std::byte{0x7c}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = raw_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
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

TEST(LzssDynamicRangeFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, single_literal_frame, staging);
    ASSERT_EQ(result.error, LzssDynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, 2U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 7U);
    constexpr std::array expected{std::byte{0x00}, std::byte{0x41}};
    EXPECT_EQ(staging, expected);
}

TEST(LzssDynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 2> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzss_dynamic_range_frame(
                      stream_for(1), {}, {}, 0, 0,
                      std::span<const std::byte>{single_literal_frame}.first(
                          size),
                      staging).error,
                  LzssDynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, extended, staging).error,
              LzssDynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(LzssDynamicRangeFrameValidator, ChecksBoundsBeforeStagingWrites) {
    std::array short_staging{std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, single_literal_frame,
                  short_staging).error,
              LzssDynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0xa5});

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 24;
    limits.max_internal_buffered_bytes = 24;
    std::array staging{std::byte{0xa5}, std::byte{0xa5}};
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream_for(1), {}, limits, 0, 0, single_literal_frame,
                  staging).error,
              LzssDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_EQ(staging[0], std::byte{0xa5});
    EXPECT_EQ(staging[1], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameValidator, RejectsImpossibleDeclaredExtents) {
    auto excessive_dictionary = single_literal_frame;
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_dictionary, 20, 3));
    std::array<std::byte, 3> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, excessive_dictionary,
                  staging).error,
              LzssDynamicRangeFrameValidationError::
                  invalid_dictionary_extent);

    std::vector<std::byte> excessive_payload(56 + 16 + 10);
    std::ranges::copy(
        std::span<const std::byte>{single_literal_frame}.first<72>(),
        excessive_payload.begin());
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        excessive_payload, 24, 10));
    std::array<std::byte, 2> token_staging{};
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, excessive_payload,
                  token_staging).error,
              LzssDynamicRangeFrameValidationError::
                  invalid_entropy_extent);
}

TEST(LzssDynamicRangeFrameValidator,
     RejectsMalformedDescriptorBeforeMutation) {
    auto malformed = single_literal_frame;
    malformed[71] = std::byte{1};
    std::array staging{std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::validate_lzss_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error,
              LzssDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(staging[0], std::byte{0xa5});
    EXPECT_EQ(staging[1], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameValidator,
     ReportsStableVariableTokenFailurePosition) {
    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto malformed = frame_for_tokens(invalid_tokens, 2);
    std::array<std::byte, invalid_tokens.size()> staging{};
    const auto result = marc::frame::validate_lzss_dynamic_range_frame(
        stream_for(2), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error, LzssDynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
    EXPECT_EQ(result.dictionary_format_error,
              marc::dictionary::internal::LzssFormatError::unknown_tag);
    EXPECT_EQ(result.dictionary_token_index, 1U);
    EXPECT_EQ(result.dictionary_input_offset, 2U);
    EXPECT_EQ(staging, invalid_tokens);
}

TEST(LzssDynamicRangeFrameValidator, RejectsTruncatedVariableToken) {
    constexpr std::array truncated_match{
        std::byte{0x01}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
    const auto malformed = frame_for_tokens(truncated_match, 2);
    std::array<std::byte, truncated_match.size()> staging{};
    const auto result = marc::frame::validate_lzss_dynamic_range_frame(
        stream_for(2), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error, LzssDynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::
                  truncated_token);
    EXPECT_EQ(result.dictionary_token_index, 0U);
    EXPECT_EQ(result.dictionary_input_offset, 0U);
}

TEST(LzssDynamicRangeFrameValidator, RejectsPipelineAndSequenceMismatch) {
    std::array<std::byte, 2> staging{};
    auto stream = stream_for(1);
    stream.entropy_block_size = 1;
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, staging).error,
              LzssDynamicRangeFrameValidationError::unsupported_pipeline);

    const auto sequence = marc::frame::validate_lzss_dynamic_range_frame(
        stream_for(1), {}, {}, 1, 0, single_literal_frame, staging);
    EXPECT_EQ(sequence.error,
              LzssDynamicRangeFrameValidationError::header_error);
    EXPECT_EQ(sequence.header_error,
              marc::frame::FrameHeaderError::unexpected_sequence);

    stream = stream_for((UINT32_C(1) << 23) + 1);
    EXPECT_EQ(marc::frame::validate_lzss_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, staging).error,
              LzssDynamicRangeFrameValidationError::unsupported_pipeline);
}

TEST(LzssDynamicRangeFrameDecoder, ReconstructsHandVectorPrivately) {
    std::array<std::byte, 2> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result =
        marc::frame::decode_lzss_dynamic_range_frame_to_staging(
            stream_for(1), {}, {}, 0, 0, single_literal_frame,
            dictionary_staging, raw_staging);
    ASSERT_EQ(result.error, LzssDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[2], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, ReconstructsOverlappingMatch) {
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
    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    const auto result =
        marc::frame::decode_lzss_dynamic_range_frame_to_staging(
            stream_for(6), {}, {}, 0, 0, frame, dictionary_staging,
            raw_staging);
    ASSERT_EQ(result.error, LzssDynamicRangeFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        raw_staging, [](const std::byte value) {
            return value == std::byte{'A'};
        }));
}

TEST(LzssDynamicRangeFrameDecoder, CapacityFailurePrecedesTokenMutation) {
    std::array dictionary_staging{std::byte{0xa5}, std::byte{0xa5}};
    const auto result =
        marc::frame::decode_lzss_dynamic_range_frame_to_staging(
            stream_for(1), {}, {}, 0, 0, single_literal_frame,
            dictionary_staging, {});
    EXPECT_EQ(result.error,
              LzssDynamicRangeFrameValidationError::raw_staging_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, IncludesRawStagingInWorkspace) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 25;
    limits.max_internal_buffered_bytes = 25;
    std::array dictionary_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    const auto result =
        marc::frame::decode_lzss_dynamic_range_frame_to_staging(
            stream_for(1), {}, limits, 0, 0, single_literal_frame,
            dictionary_staging, raw_staging);
    EXPECT_EQ(result.error,
              LzssDynamicRangeFrameValidationError::workspace_limit);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, MalformedLayersDoNotTouchRawStaging) {
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    auto malformed_descriptor = single_literal_frame;
    malformed_descriptor[71] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lzss_dynamic_range_frame_to_staging(
                  stream_for(1), {}, {}, 0, 0, malformed_descriptor,
                  dictionary_staging, raw_staging).error,
              LzssDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    EXPECT_EQ(marc::frame::decode_lzss_dynamic_range_frame_to_staging(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  dictionary_staging, raw_staging).error,
              LzssDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
    EXPECT_EQ(raw_staging[1], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, PublishesHandVectorAfterPrivateDecode) {
    std::array<std::byte, 2> dictionary_staging{};
    std::array raw_staging{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, single_literal_frame,
        dictionary_staging, raw_staging, output);
    ASSERT_EQ(result.error, LzssDynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xa5});
    EXPECT_EQ(output[2], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, PublishesOverlappingMatchAtomically) {
    std::array<std::byte, 11> tokens{};
    std::size_t written{};
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::literal,
                   0, 0, 'A'},
                  tokens, written),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_token(
                  {marc::dictionary::internal::LzssTokenTag::match,
                   1, 5, 0},
                  std::span<std::byte>{tokens}.subspan(written), written),
              marc::dictionary::internal::LzssFormatError::none);
    const auto frame = frame_for_tokens(tokens, 6);

    std::array<std::byte, tokens.size()> dictionary_staging{};
    std::array<std::byte, 6> raw_staging{};
    std::array output{
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}, std::byte{0xa5}, std::byte{0xa5},
        std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_dynamic_range_frame(
        stream_for(6), {}, {}, 0, 0, frame, dictionary_staging,
        raw_staging, output);
    ASSERT_EQ(result.error, LzssDynamicRangeFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.first<6>(),
        [](const std::byte value) {
            return value == std::byte{'A'};
        }));
    EXPECT_EQ(output[6], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, OutputCapacityFailurePrecedesMutation) {
    std::array dictionary_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array raw_staging{std::byte{0xa5}};
    const auto result = marc::frame::decode_lzss_dynamic_range_frame(
        stream_for(1), {}, {}, 0, 0, single_literal_frame,
        dictionary_staging, raw_staging, {});
    EXPECT_EQ(result.error,
              LzssDynamicRangeFrameValidationError::raw_output_too_small);
    EXPECT_EQ(dictionary_staging[0], std::byte{0xa5});
    EXPECT_EQ(dictionary_staging[1], std::byte{0xa5});
    EXPECT_EQ(raw_staging[0], std::byte{0xa5});
}

TEST(LzssDynamicRangeFrameDecoder, MalformedFrameNeverPublishesOutput) {
    std::array<std::byte, 4> dictionary_staging{};
    std::array raw_staging{std::byte{0xa5}, std::byte{0xa5}};
    std::array output{std::byte{0xa5}, std::byte{0xa5}};
    auto malformed_descriptor = single_literal_frame;
    malformed_descriptor[71] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lzss_dynamic_range_frame(
                  stream_for(1), {}, {}, 0, 0, malformed_descriptor,
                  dictionary_staging, raw_staging, output).error,
              LzssDynamicRangeFrameValidationError::descriptor_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});

    constexpr std::array invalid_tokens{
        std::byte{0x00}, std::byte{0x41},
        std::byte{0x02}, std::byte{0x42}};
    const auto invalid_dictionary = frame_for_tokens(invalid_tokens, 2);
    EXPECT_EQ(marc::frame::decode_lzss_dynamic_range_frame(
                  stream_for(2), {}, {}, 0, 0, invalid_dictionary,
                  dictionary_staging, raw_staging, output).error,
              LzssDynamicRangeFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0xa5});
    EXPECT_EQ(output[1], std::byte{0xa5});
}

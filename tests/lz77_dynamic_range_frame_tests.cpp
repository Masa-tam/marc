#include "frame/lz77_dynamic_range_frame.hpp"

#include "core/endian.hpp"
#include "dictionary/lz77_format.hpp"
#include "entropy/dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::Lz77DynamicRangeFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::uint32_t raw_size) {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
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

constexpr std::array<std::byte, 88> single_literal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x06}, std::byte{0x5c}, std::byte{0xd6},
    std::byte{0x5f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

[[nodiscard]] std::vector<std::byte> frame_for_tokens(
    const std::span<const std::byte> tokens,
    const std::uint32_t raw_size = 1) {
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
                      marc::entropy::internal::dynamic_range_descriptor_size}),
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

TEST(Lz77DynamicRangeFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_dynamic_range_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, staging);
    ASSERT_EQ(result.error, Lz77DynamicRangeFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.dictionary_size, 16U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.payload_size, 16U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[12], std::byte{0x41});
}

TEST(Lz77DynamicRangeFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<std::byte, 16> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lz77_dynamic_range_frame(
                      stream_for_a(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_literal_frame}.first(
                          size),
                      staging).error,
                  Lz77DynamicRangeFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, staging).error,
              Lz77DynamicRangeFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77DynamicRangeFrameValidator, RejectsShortStagingBeforeMutation) {
    std::array<std::byte, 15> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  staging).error,
              Lz77DynamicRangeFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77DynamicRangeFrameValidator,
    EnforcesAggregateWorkspaceBeforeMutation) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 16 + 16 + 16 - 1;
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  staging).error,
              Lz77DynamicRangeFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77DynamicRangeFrameValidator,
     RejectsMalformedDescriptorBeforeMutation) {
    auto malformed = single_literal_frame;
    malformed[71] = std::byte{1};
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_dynamic_range_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error,
              Lz77DynamicRangeFrameValidationError::descriptor_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77DynamicRangeFrameValidator, RejectsNoncanonicalRangePayload) {
    auto malformed = single_literal_frame;
    malformed[72] = std::byte{1};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_dynamic_range_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error,
              Lz77DynamicRangeFrameValidationError::entropy_decode_error);
    EXPECT_EQ(result.entropy_error,
              marc::entropy::internal::DynamicRangeDecodeError::
                  invalid_interval);
}

TEST(Lz77DynamicRangeFrameValidator,
     RejectsEntropyDecodedInvalidLz77Token) {
    std::array<std::byte, 16> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed = frame_for_tokens(invalid_tokens);
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_dynamic_range_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, staging);
    EXPECT_EQ(result.error, Lz77DynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77DynamicRangeFrameValidator, RejectsImpossibleEntropyExtentEarly) {
    std::vector<std::byte> malformed(56 + 16 + 38);
    std::ranges::copy(
        std::span<const std::byte>{single_literal_frame}.first<72>(),
        malformed.begin());
    ASSERT_TRUE(marc::core::store_le(
        std::span<std::byte>{malformed}, 24, std::uint32_t{38}));
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream_for_a(), {}, {}, 0, 0, malformed, staging).error,
              Lz77DynamicRangeFrameValidationError::invalid_entropy_extent);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77DynamicRangeFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  staging).error,
              Lz77DynamicRangeFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lz77_dynamic_range_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, staging).error,
              Lz77DynamicRangeFrameValidationError::unsupported_pipeline);
}

TEST(Lz77DynamicRangeFrameDecoder, ReconstructsHandVectorIntoPrivateStaging) {
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 3> raw_staging{};
    raw_staging.fill(std::byte{0x5a});
    const auto result =
        marc::frame::decode_lz77_dynamic_range_frame_to_staging(
            stream_for_a(), {}, {}, 0, 0, single_literal_frame,
            dictionary_staging, raw_staging);
    ASSERT_EQ(result.error, Lz77DynamicRangeFrameValidationError::none);
    EXPECT_EQ(raw_staging[0], std::byte{'A'});
    EXPECT_EQ(raw_staging[1], std::byte{0x5a});
    EXPECT_EQ(raw_staging[2], std::byte{0x5a});
}

TEST(Lz77DynamicRangeFrameDecoder, ReconstructsOverlappingMatch) {
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
    const auto result =
        marc::frame::decode_lz77_dynamic_range_frame_to_staging(
            stream_for(5), {}, {}, 0, 0, encoded, dictionary_staging,
            raw_staging);
    ASSERT_EQ(result.error, Lz77DynamicRangeFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(raw_staging, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(Lz77DynamicRangeFrameDecoder,
     RejectsShortRawStagingBeforeTokenMutation) {
    std::array<std::byte, 16> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    const auto result =
        marc::frame::decode_lz77_dynamic_range_frame_to_staging(
            stream_for_a(), {}, {}, 0, 0, single_literal_frame,
            dictionary_staging, {});
    EXPECT_EQ(result.error,
              Lz77DynamicRangeFrameValidationError::raw_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
}

TEST(Lz77DynamicRangeFrameDecoder, IncludesRawStagingInAggregateLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 16 + 16 + 16 + 1 - 1;
    std::array<std::byte, 16> dictionary_staging{};
    dictionary_staging.fill(std::byte{0x5a});
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    const auto result =
        marc::frame::decode_lz77_dynamic_range_frame_to_staging(
            stream_for_a(), {}, limits, 0, 0, single_literal_frame,
            dictionary_staging, raw_staging);
    EXPECT_EQ(result.error,
              Lz77DynamicRangeFrameValidationError::workspace_limit);
    EXPECT_TRUE(std::ranges::all_of(
        dictionary_staging, [](const std::byte value) {
            return value == std::byte{0x5a};
        }));
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

TEST(Lz77DynamicRangeFrameDecoder, MalformedTokensNeverMutateRawStaging) {
    std::array<std::byte, 16> invalid_tokens{};
    invalid_tokens[0] = std::byte{0xff};
    const auto malformed = frame_for_tokens(invalid_tokens);
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 1> raw_staging{std::byte{0x5a}};
    const auto result =
        marc::frame::decode_lz77_dynamic_range_frame_to_staging(
            stream_for_a(), {}, {}, 0, 0, malformed, dictionary_staging,
            raw_staging);
    EXPECT_EQ(result.error, Lz77DynamicRangeFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(raw_staging[0], std::byte{0x5a});
}

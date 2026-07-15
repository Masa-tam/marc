#include "frame/lz77_blocked_huffman_frame.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::frame::Lz77BlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = 16;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] marc::frame::StreamHeader stream_for_repeated_a() {
    auto stream = stream_for_a();
    stream.frame_size = 5;
    stream.entropy_block_size = 32;
    stream.original_size = 5;
    return stream;
}

[[nodiscard]] std::array<std::byte, 104> repeated_a_frame() {
    std::array<std::byte, 104> encoded{};
    const auto stream = stream_for_repeated_a();
    const marc::core::DecoderLimits limits{};

    marc::frame::FrameHeader header{};
    header.uncompressed_size = 5;
    header.dictionary_serialized_size = 32;
    header.compressed_payload_size = 32;
    header.entropy_block_count = 1;
    header.block_descriptors_size = 16;
    const marc::frame::FrameValidationContext context{stream, limits, 0, 0};
    std::span<std::byte, marc::frame::frame_header_size> header_output{
        encoded.data(), marc::frame::frame_header_size};
    EXPECT_EQ(marc::frame::serialize_frame_header(
                  header, context, header_output),
              marc::frame::FrameHeaderError::none);

    const marc::entropy::internal::BlockedHuffmanDescriptor descriptor{
        32, 32, 0, marc::entropy::internal::blocked_huffman_raw_flag, 8};
    std::span<std::byte,
              marc::entropy::internal::blocked_huffman_descriptor_size>
        descriptor_output{encoded.data() + marc::frame::frame_header_size,
                          marc::entropy::internal::
                              blocked_huffman_descriptor_size};
    EXPECT_EQ(marc::entropy::internal::serialize_block_descriptor(
                  descriptor, 32, limits, descriptor_output),
              marc::entropy::internal::BlockedHuffmanFormatError::none);

    const auto payload_offset = marc::frame::frame_header_size
        + marc::entropy::internal::blocked_huffman_descriptor_size;
    const marc::dictionary::internal::Lz77Token literal{
        marc::dictionary::internal::Lz77TokenTag::literal, 0, 0, 'A'};
    const marc::dictionary::internal::Lz77Token match{
        marc::dictionary::internal::Lz77TokenTag::terminal_match, 1, 4, 0};
    std::span<std::byte, marc::dictionary::internal::lz77_token_size>
        literal_output{encoded.data() + payload_offset,
                       marc::dictionary::internal::lz77_token_size};
    std::span<std::byte, marc::dictionary::internal::lz77_token_size>
        match_output{encoded.data() + payload_offset
                         + marc::dictionary::internal::lz77_token_size,
                     marc::dictionary::internal::lz77_token_size};
    EXPECT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  literal, literal_output),
              marc::dictionary::internal::Lz77FormatError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lz77_token(
                  match, match_output),
              marc::dictionary::internal::Lz77FormatError::none);
    return encoded;
}

constexpr std::array<std::byte, 88> single_literal_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0}};

} // namespace

TEST(Lz77BlockedHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.dictionary_size, 16U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[12], std::byte{0x41});
}

TEST(Lz77BlockedHuffmanFrameValidator, StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
            stream_for_a(), {}, {}, 0, 0,
            std::span<const std::byte>{single_literal_frame}.first(size),
            views, staging);
        EXPECT_NE(result.error, Lz77BlockedHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsWorkspaceCapacityBeforeWritingStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 15> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  views, short_staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  std::span<marc::entropy::internal::BlockedHuffmanBlockView>{},
                  staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  view_output_too_small);
}

TEST(Lz77BlockedHuffmanFrameValidator, EnforcesAggregateWorkspaceLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    const std::uint64_t required = 16 + 16 + 16
        + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsEntropyMetadataBeforeStaging) {
    auto malformed = single_literal_frame;
    malformed[64] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error,
              Lz77BlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsInvalidStagedLz77Token) {
    auto malformed = single_literal_frame;
    malformed[72] = std::byte{0xff};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(Lz77BlockedHuffmanFrameDecoder, DecodesHandVectorWithoutTouchingTail) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 3> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging,
        output);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(Lz77BlockedHuffmanFrameDecoder, DecodesOverlappingMatch) {
    const auto encoded = repeated_a_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 32> staging{};
    std::array<std::byte, 5> output{};
    const auto result = marc::frame::decode_lz77_blocked_huffman_frame(
        stream_for_repeated_a(), {}, {}, 0, 0, encoded, views, staging,
        output);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(Lz77BlockedHuffmanFrameDecoder, ShortRawOutputIsAtomic) {
    const auto encoded = repeated_a_frame();
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 32> staging{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lz77_blocked_huffman_frame(
        stream_for_repeated_a(), {}, {}, 0, 0, encoded, views, staging,
        output);
    EXPECT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::
                                raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77BlockedHuffmanFrameDecoder, MalformedLayersDoNotPublishRawBytes) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 1> output{std::byte{0x5a}};

    auto invalid_descriptor = single_literal_frame;
    invalid_descriptor[64] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_descriptor, views,
                  staging, output)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::controller_error);
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto invalid_token = single_literal_frame;
    invalid_token[72] = std::byte{0xff};
    EXPECT_EQ(marc::frame::decode_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_token, views,
                  staging, output)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(Lz77BlockedHuffmanFrameEncoder, PlansAndEmitsExactHandVector) {
    const std::array raw{std::byte{'A'}};
    std::array<std::byte, 16> staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.raw_size, 1U);
    EXPECT_EQ(plan.dictionary_size, 16U);
    EXPECT_EQ(plan.descriptor_size, 16U);
    EXPECT_EQ(plan.payload_size, 16U);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.serialized_size, single_literal_frame.size());

    std::array<std::byte, single_literal_frame.size()> encoded{};
    const auto result = marc::frame::encode_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, encoded);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(encoded, single_literal_frame);
}

TEST(Lz77BlockedHuffmanFrameEncoder, RoundTripsOverlappingMatchDeterministically) {
    const std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}};
    std::array<std::byte, 32> encode_staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_frame(
        stream_for_repeated_a(), {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(plan.serialized_size, 104U);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_frame(
                  stream_for_repeated_a(), {}, {}, 0, 0, raw,
                  encode_staging, first)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_frame(
                  stream_for_repeated_a(), {}, {}, 0, 0, raw,
                  encode_staging, second)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 32> decode_staging{};
    std::array<std::byte, 5> decoded{};
    ASSERT_EQ(marc::frame::decode_lz77_blocked_huffman_frame(
                  stream_for_repeated_a(), {}, {}, 0, 0, first, views,
                  decode_staging, decoded)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(Lz77BlockedHuffmanFrameEncoder, UsesCanonicalHuffmanWhenItIsSmaller) {
    std::array<std::byte, 64> raw{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = static_cast<std::byte>(index);
    }
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 1024;
    std::array<std::byte, 1024> encode_staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(plan.dictionary_size, 1024U);
    EXPECT_EQ(plan.descriptor_size, 272U);
    EXPECT_LT(plan.payload_size, plan.dictionary_size);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, encoded)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 1024> decode_staging{};
    std::array<std::byte, 64> decoded{};
    ASSERT_EQ(marc::frame::decode_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, encoded, views, decode_staging,
                  decoded)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
    EXPECT_EQ(views[0].descriptor.flags, 0U);
}

TEST(Lz77BlockedHuffmanFrameEncoder, CapacityFailuresDoNotModifyDestinations) {
    const std::array raw{std::byte{'A'}};
    std::array<std::byte, 15> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::plan_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw, short_staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<std::byte, 16> staging{};
    std::array<std::byte, 87> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, short_output);
    EXPECT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, 88U);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77BlockedHuffmanFrameEncoder, RoundTripsMultipleEntropyBlocksAndFinalShortBlock) {
    std::array<std::byte, 64> raw{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = static_cast<std::byte>(index);
    }
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 300;
    std::array<std::byte, 1024> encode_staging{};
    const auto plan = marc::frame::plan_lz77_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.dictionary_size, 1024U);
    EXPECT_EQ(plan.block_count, 4U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, encoded)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 4> views{};
    std::array<std::byte, 1024> decode_staging{};
    std::array<std::byte, 64> decoded{};
    ASSERT_EQ(marc::frame::decode_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, encoded, views, decode_staging,
                  decoded)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(decoded, raw);
    EXPECT_EQ(views[3].descriptor.symbol_count, 124U);
}

TEST(Lz77BlockedHuffmanFrameEncoder, RejectsEmptyAndUnexpectedFrameExtent) {
    std::array<std::byte, 32> staging{};
    EXPECT_EQ(marc::frame::plan_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0,
                  std::span<const std::byte>{}, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::input_size_mismatch);

    const std::array raw{std::byte{'A'}, std::byte{'B'}};
    EXPECT_EQ(marc::frame::plan_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::input_size_mismatch);
}

#include "frame/lzss_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzssBlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = 1;
    return stream;
}

constexpr std::array<std::byte, 74> single_literal_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0x41}};

} // namespace

TEST(LzssBlockedHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging);
    ASSERT_EQ(result.error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.dictionary_size, 2U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[1], std::byte{0x41});
}

TEST(LzssBlockedHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
            stream_for_a(), {}, {}, 0, 0,
            std::span<const std::byte>{single_literal_frame}.first(size),
            views, staging);
        EXPECT_NE(result.error,
                  LzssBlockedHuffmanFrameValidationError::none) << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzssBlockedHuffmanFrameValidator,
     RejectsWorkspaceCapacityBeforeWritingStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 1> short_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  views, short_staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0x5a});

    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  std::span<marc::entropy::internal::
                      BlockedHuffmanBlockView>{},
                  staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::view_output_too_small);
}

TEST(LzssBlockedHuffmanFrameValidator, EnforcesAggregateWorkspaceLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 2;
    const std::uint64_t required = 16 + 2 + 2
        + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(LzssBlockedHuffmanFrameValidator,
     RejectsEntropyMetadataBeforeWritingStaging) {
    auto malformed = single_literal_frame;
    malformed[64] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error,
              LzssBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssBlockedHuffmanFrameValidator, RejectsInvalidStagedLzssToken) {
    auto malformed = single_literal_frame;
    malformed[72] = std::byte{0xff};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error, LzssBlockedHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(LzssBlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

TEST(LzssBlockedHuffmanFrameDecoder, DecodesHandVectorWithoutTouchingTail) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 3> output{};
    output.fill(std::byte{0x5a});
    const auto result = marc::frame::decode_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging,
        output);
    ASSERT_EQ(result.error, LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0x5a});
    EXPECT_EQ(output[2], std::byte{0x5a});
}

TEST(LzssBlockedHuffmanFrameDecoder, DecodesOverlappingMatch) {
    const std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 32;
    std::array<std::byte, 12> encode_staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, encoded)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 12> decode_staging{};
    std::array<std::byte, raw.size()> output{};
    ASSERT_EQ(marc::frame::decode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, encoded, views, decode_staging,
                  output)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output, raw);
}

TEST(LzssBlockedHuffmanFrameDecoder, ShortRawOutputIsAtomic) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 0> output{};
    const auto result = marc::frame::decode_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging,
        output);
    EXPECT_EQ(result.error,
              LzssBlockedHuffmanFrameValidationError::raw_output_too_small);

    const std::array repeated_raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(repeated_raw.size());
    stream.original_size = repeated_raw.size();
    stream.entropy_block_size = 32;
    std::array<std::byte, 12> encode_staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, repeated_raw, encode_staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, repeated_raw, encode_staging,
                  encoded)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    std::array<std::byte, 12> decode_staging{};
    std::array<std::byte, 5> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto short_result = marc::frame::decode_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, encoded, views, decode_staging, short_output);
    EXPECT_EQ(short_result.error,
              LzssBlockedHuffmanFrameValidationError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssBlockedHuffmanFrameDecoder, MalformedLayersDoNotPublishRawBytes) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    std::array<std::byte, 1> output{std::byte{0x5a}};

    auto invalid_descriptor = single_literal_frame;
    invalid_descriptor[64] = std::byte{1};
    EXPECT_EQ(marc::frame::decode_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_descriptor, views,
                  staging, output)
                  .error,
              LzssBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto invalid_token = single_literal_frame;
    invalid_token[72] = std::byte{0xff};
    EXPECT_EQ(marc::frame::decode_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, invalid_token, views,
                  staging, output)
                  .error,
              LzssBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
    EXPECT_EQ(output[0], std::byte{0x5a});
}

TEST(LzssBlockedHuffmanFrameEncoder, PlansAndEmitsExactHandVector) {
    const std::array raw{std::byte{'A'}};
    std::array<std::byte, 2> staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.raw_size, 1U);
    EXPECT_EQ(plan.dictionary_size, 2U);
    EXPECT_EQ(plan.descriptor_size, 16U);
    EXPECT_EQ(plan.payload_size, 2U);
    EXPECT_EQ(plan.block_count, 1U);
    EXPECT_EQ(plan.serialized_size, single_literal_frame.size());

    std::array<std::byte, single_literal_frame.size()> encoded{};
    const auto result = marc::frame::encode_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, encoded);
    ASSERT_EQ(result.error, LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(encoded, single_literal_frame);
}

TEST(LzssBlockedHuffmanFrameEncoder,
     DeterministicallyRoundTripsVariableTokenStaging) {
    const std::array raw{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 32;
    std::array<std::byte, 12> encode_staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(plan.dictionary_size, 11U);
    std::vector<std::byte> first(plan.serialized_size);
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, first)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, second)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(first, second);

    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 12> decode_staging{};
    const auto validated = marc::frame::validate_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, first, views, decode_staging);
    ASSERT_EQ(validated.error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{decode_staging}.first(
            validated.dictionary_size),
        std::span<const std::byte>{encode_staging}.first(
            plan.dictionary_size)));
}

TEST(LzssBlockedHuffmanFrameEncoder, UsesCanonicalHuffmanWhenSmaller) {
    std::array<std::byte, 1024> raw{};
    std::uint32_t state = 0x6d617263U;
    for (auto& value : raw) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        value = static_cast<std::byte>(state & 0xffU);
    }
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 2048;
    std::array<std::byte, 2048> encode_staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, encode_staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.descriptor_size, 272U);
    EXPECT_LT(plan.payload_size, plan.dictionary_size);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, encode_staging, encoded)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2048> decode_staging{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(marc::frame::decode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, encoded, views, decode_staging,
                  decoded)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(views[0].descriptor.flags, 0U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssBlockedHuffmanFrameEncoder,
     CapacityFailuresDoNotModifyDestinations) {
    const std::array raw{std::byte{'A'}};
    std::array<std::byte, 1> short_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::plan_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw, short_staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0x5a});

    std::array<std::byte, 2> staging{};
    std::array<std::byte, single_literal_frame.size() - 1> short_output{};
    short_output.fill(std::byte{0x5a});
    const auto result = marc::frame::encode_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, raw, staging, short_output);
    EXPECT_EQ(result.error, LzssBlockedHuffmanFrameValidationError::
                                serialized_output_too_small);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssBlockedHuffmanFrameEncoder,
     PlansMultipleBlocksAndRejectsUnexpectedFrameExtent) {
    std::array<std::byte, 64> raw{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = static_cast<std::byte>(index);
    }
    auto stream = stream_for_a();
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.original_size = raw.size();
    stream.entropy_block_size = 30;
    std::array<std::byte, 128> staging{};
    const auto plan = marc::frame::plan_lzss_blocked_huffman_frame(
        stream, {}, {}, 0, 0, raw, staging);
    ASSERT_EQ(plan.error, LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(plan.dictionary_size, 128U);
    EXPECT_EQ(plan.block_count, 5U);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, raw, staging, encoded)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 5> views{};
    std::array<std::byte, 128> decoded_staging{};
    ASSERT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, encoded, views, decoded_staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(views[4].descriptor.symbol_count, 8U);

    EXPECT_EQ(marc::frame::plan_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{}, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::input_size_mismatch);
    EXPECT_EQ(marc::frame::plan_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, raw, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::input_size_mismatch);
}

#include "frame/lzss_short_length_escape_frame_decoder.hpp"

#include "context/lzss_short_length_escape.hpp"
#include "core/endian.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] TypedContextStreamHeader stream_for(const std::uint32_t raw) {
    TypedContextStreamHeader stream{};
    stream.frame_size = raw;
    stream.original_size = raw;
    stream.dictionary = {65536, 3, 258, 0};
    stream.range_model_total = typed_context_model_total;
    stream.context_count = 32;
    stream.dictionary_variant = 8;
    stream.context_variant = 7;
    return stream;
}

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
stream_header_bytes() {
    std::array<std::byte, typed_context_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    const std::span<std::byte> output{bytes};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 8, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 10, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(output, 12, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 14, std::uint16_t{8}));
    EXPECT_TRUE(marc::core::store_le(output, 16, std::uint16_t{3}));
    EXPECT_TRUE(marc::core::store_le(output, 18, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 20, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(output, 28, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 32, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 40, std::uint64_t{4}));
    EXPECT_TRUE(marc::core::store_le(output, 48, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 64, std::uint32_t{65536}));
    EXPECT_TRUE(marc::core::store_le(output, 68, std::uint32_t{3}));
    EXPECT_TRUE(marc::core::store_le(output, 72, std::uint32_t{258}));
    EXPECT_TRUE(marc::core::store_le(output, 80, typed_context_model_total));
    EXPECT_TRUE(marc::core::store_le(output, 84, std::uint16_t{32}));
    EXPECT_TRUE(marc::core::store_le(output, 96, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(output, 98, std::uint16_t{7}));
    return bytes;
}

[[nodiscard]] std::vector<std::byte> frame_for(const std::uint32_t length) {
    const auto field = marc::context::internal::
        encode_lzss_short_length_escape(length);
    std::vector<ModeledOperation> operations{
        {ModeledOperationKind::symbol, 0, 2, 0, 0},
        {ModeledOperationKind::symbol, 3, 256, 0x61, 0},
        {ModeledOperationKind::symbol, 1, 2, 1, 0},
        {ModeledOperationKind::symbol, 21, 9, field.length_class, 0},
    };
    if (field.bit_count != 0) {
        operations.push_back({ModeledOperationKind::bypass_bits, 0, 0,
                              field.extra, field.bit_count});
    }
    operations.push_back({ModeledOperationKind::symbol,
                          static_cast<std::uint16_t>(23 + field.length_class),
                          17, 0, 0});
    const auto limits = marc::core::DecoderLimits{};
    marc::entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::
        plan_lzss_short_match_range_operations(
            operations, limits, descriptor);
    EXPECT_EQ(plan.error, marc::entropy::internal::
                              ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    const auto encoded = marc::entropy::internal::
        encode_lzss_short_match_range_operations(
            operations, limits, payload, descriptor);
    EXPECT_EQ(encoded.error, marc::entropy::internal::
                                 ContextualDynamicRangeEncodeError::none);

    std::vector<std::byte> frame(typed_context_frame_header_size
                                 + typed_context_range_descriptor_size
                                 + payload.size());
    frame[0] = std::byte{0x4d};
    frame[1] = std::byte{0x52};
    frame[2] = std::byte{0x46};
    frame[3] = std::byte{0x32};
    const std::span<std::byte> output{frame};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 16, length + 1));
    EXPECT_TRUE(marc::core::store_le(output, 20, std::uint32_t{2}));
    EXPECT_TRUE(marc::core::store_le(
        output, 24, static_cast<std::uint32_t>(operations.size())));
    EXPECT_TRUE(marc::core::store_le(
        output, 28, descriptor.decision_count));
    EXPECT_TRUE(marc::core::store_le(
        output, 32, static_cast<std::uint32_t>(payload.size())));
    EXPECT_TRUE(marc::core::store_le(output, 36, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 64,
                                     descriptor.decision_count));
    EXPECT_TRUE(marc::core::store_le(output, 68,
                                     static_cast<std::uint32_t>(payload.size())));
    EXPECT_TRUE(marc::core::store_le(output, 72, std::uint16_t{32}));
    for (std::size_t index = 0; index < payload.size(); ++index) {
        frame[80 + index] = payload[index];
    }
    return frame;
}

} // namespace

TEST(LzssShortLengthEscapePreflight, ParsesOnlyItsPrivateIdentity) {
    const auto limits = marc::core::DecoderLimits{};
    auto bytes = stream_header_bytes();
    TypedContextStreamHeader parsed{};
    std::size_t consumed = 777;
    EXPECT_EQ(parse_lzss_short_length_escape_stream_header(
                  std::span{bytes}.first(111), limits, parsed, consumed),
              LzssShortMatchPreflightError::truncated_stream_header);
    EXPECT_EQ(consumed, 777U);
    ASSERT_EQ(parse_lzss_short_length_escape_stream_header(
                  bytes, limits, parsed, consumed),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(consumed, 112U);
    EXPECT_EQ(parsed.dictionary_variant, 8U);
    EXPECT_EQ(parsed.context_variant, 7U);
    EXPECT_EQ(validate_lzss_short_length_escape_stream_semantics(
                  parsed, limits), LzssShortMatchPreflightError::none);
    TypedContextStreamHeader published{};
    std::size_t published_consumed = 777;
    EXPECT_EQ(parse_typed_context_stream_header(
                  bytes, limits, published, published_consumed),
              TypedContextStreamHeaderError::unsupported_dictionary_variant);
    EXPECT_EQ(published_consumed, 777U);
    EXPECT_EQ(parse_lzss_short_match_stream_header(
                  bytes, limits, parsed, consumed),
              LzssShortMatchPreflightError::unsupported_format);

    EXPECT_TRUE(marc::core::store_le(std::span{bytes}, 98,
                                     std::uint16_t{6}));
    EXPECT_EQ(parse_lzss_short_length_escape_stream_header(
                  bytes, limits, parsed, consumed),
              LzssShortMatchPreflightError::unsupported_format);
    EXPECT_EQ(consumed, 112U);
}

TEST(LzssShortLengthEscapeFrameDecoder, ReconstructsLengthBoundaries) {
    const auto limits = marc::core::DecoderLimits{};
    for (const auto length : {3U, 4U, 5U, 258U}) {
        const auto frame = frame_for(length);
        const auto stream = stream_for(length + 1);
        const TypedContextFrameValidationContext context{stream, limits, 0, 0};
        TypedContextFrameLayout layout{};
        LzssShortMatchFrameRequirements requirements{};
        ASSERT_EQ(preflight_lzss_short_length_escape_frame_bytes(
                      frame, context, layout, requirements),
                  LzssShortMatchPreflightError::none) << length;
        EXPECT_EQ(layout.serialized_size, frame.size());
        EXPECT_EQ(requirements.token_count, 2U);
        EXPECT_EQ(requirements.raw_frame_bytes, length + 1);
        std::array<LzssTypedToken, 2> tokens{};
        std::vector<std::byte> raw(length + 1);
        const auto result = decode_lzss_short_length_escape_frame(
            frame, context, tokens, raw);
        ASSERT_EQ(result.error, LzssShortMatchFrameDecodeError::none)
            << length;
        EXPECT_EQ(result.serialized_consumed, frame.size());
        EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
        EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
        EXPECT_EQ(tokens[1].distance, 1U);
        EXPECT_EQ(tokens[1].length, length);
        for (const auto value : raw) EXPECT_EQ(value, std::byte{0x61});

        EXPECT_EQ(preflight_lzss_short_match_frame_bytes(
                      frame, context, layout, requirements),
                  LzssShortMatchPreflightError::invalid_stream);
    }
}

TEST(LzssShortLengthEscapeFrameDecoder, RejectsMalformedWithoutRawOutput) {
    auto frame = frame_for(3);
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> tokens{};
    tokens[0].literal = 0xa5;
    std::array<std::byte, 4> raw{};
    raw.fill(std::byte{0xcc});

    auto result = decode_lzss_short_length_escape_frame(
        std::span{frame}.first(frame.size() - 1), context, tokens, raw);
    EXPECT_EQ(result.error, LzssShortMatchFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight_error,
              LzssShortMatchPreflightError::truncated_frame);
    EXPECT_EQ(result.serialized_consumed, 0U);

    frame[48] = std::byte{1};
    result = decode_lzss_short_length_escape_frame(
        frame, context, tokens, raw);
    EXPECT_EQ(result.preflight_error,
              LzssShortMatchPreflightError::nonzero_reserved);
    frame[48] = std::byte{0};
    frame[80] = std::byte{1};
    result = decode_lzss_short_length_escape_frame(
        frame, context, tokens, raw);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameDecodeError::token_decode_error);
    EXPECT_EQ(tokens[0].literal, 0xa5U);
    for (const auto value : raw) EXPECT_EQ(value, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeFrameDecoder, ChecksCapacitiesOverlapAndSequence) {
    auto frame = frame_for(3);
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw{};
    EXPECT_EQ(decode_lzss_short_length_escape_frame(
                  frame, context, std::span{tokens}.first(1), raw).error,
              LzssShortMatchFrameDecodeError::token_output_too_small);
    EXPECT_EQ(decode_lzss_short_length_escape_frame(
                  frame, context, tokens, std::span{raw}.first(3)).error,
              LzssShortMatchFrameDecodeError::raw_output_too_small);
    EXPECT_EQ(decode_lzss_short_length_escape_frame(
                  frame, context, tokens, std::span{frame}.first(4)).error,
              LzssShortMatchFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(decode_lzss_short_length_escape_frame(
                  frame, context, tokens,
                  std::span<std::byte>{
                      reinterpret_cast<std::byte*>(tokens.data()),
                      sizeof(tokens)}.first(4)).error,
              LzssShortMatchFrameDecodeError::overlapping_workspaces);

    EXPECT_TRUE(marc::core::store_le(std::span{frame}, 8,
                                     std::uint64_t{1}));
    EXPECT_EQ(decode_lzss_short_length_escape_frame(
                  frame, context, tokens, raw).preflight_error,
              LzssShortMatchPreflightError::unexpected_sequence);

    frame = frame_for(3);
    TypedContextFrameLayout layout{};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_short_length_escape_frame_bytes(
                  frame, context, layout, requirements),
              LzssShortMatchPreflightError::none);
    auto strict = limits;
    strict.max_block_size = 4;
    strict.max_internal_buffered_bytes =
        requirements.aggregate_working_bytes - 1;
    EXPECT_EQ(preflight_lzss_short_length_escape_frame_bytes(
                  frame, {stream, strict, 0, 0}, layout, requirements),
              LzssShortMatchPreflightError::limit_exceeded);
}

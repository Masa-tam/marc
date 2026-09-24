#include "context/lzss_field_context_format.hpp"
#include "context/lzss_short_match_context_layout.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "frame/lzss_short_match_preflight.hpp"
#include "core/endian.hpp"
#include "entropy/lzss_short_match_range_decoder.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <array>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] TypedContextStreamHeader short_stream() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 4;
    stream.original_size = 4;
    stream.dictionary.min_match_length = 3;
    stream.range_model_total = typed_context_model_total;
    stream.context_count =
        marc::context::internal::lzss_short_match_context_count;
    stream.dictionary_variant = 7;
    stream.context_variant = 6;
    return stream;
}

[[nodiscard]] constexpr TypedContextFrameHeader short_frame() {
    return {0, 0, 4, 2, 5, 5, 7, 16, 0, 0};
}

[[nodiscard]] constexpr TypedContextRangeDescriptor short_descriptor() {
    return {5, 7, 32};
}

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
short_stream_bytes() {
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
    EXPECT_TRUE(marc::core::store_le(output, 14, std::uint16_t{7}));
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
    EXPECT_TRUE(marc::core::store_le(output, 98, std::uint16_t{6}));
    return bytes;
}

[[nodiscard]] std::array<std::byte, 87> short_frame_bytes() {
    std::array<std::byte, 87> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    const std::span<std::byte> output{bytes};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 16, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(output, 20, std::uint32_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 24, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 28, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 32, std::uint32_t{7}));
    EXPECT_TRUE(marc::core::store_le(output, 36, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 64, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 68, std::uint32_t{7}));
    EXPECT_TRUE(marc::core::store_le(output, 72, std::uint16_t{32}));
    return bytes;
}

} // namespace

TEST(LzssShortMatchContextLayout, AddsOnlyTheNinthDistanceContext) {
    using namespace marc::context::internal;
    EXPECT_EQ(lzss_field_context_count, 31U);
    EXPECT_EQ(lzss_field_context_frequency_entries_v1, 4518U);
    EXPECT_EQ(lzss_short_match_context_count, 32U);
    EXPECT_EQ(lzss_short_match_frequency_entries, 4538U);
    EXPECT_EQ(lzss_short_match_offsets[20], 4358U);
    EXPECT_EQ(lzss_short_match_offsets[23], 4385U);
    EXPECT_EQ(lzss_short_match_offsets[32], 4538U);
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 2U);
    }
    for (std::size_t index = 3; index < 20; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 256U);
    }
    for (std::size_t index = 20; index < 23; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 9U);
    }
    for (std::size_t index = 23; index < 32; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 17U);
    }
}

TEST(LzssShortMatchPreflight, ValidatesOnlyTheReservedStreamSemantics) {
    auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::none);

    stream.context_count = 31;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.dictionary_variant = 6;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.context_variant = 5;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.dictionary_variant = 8;
    stream.context_variant = 7;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.frame_size = 65537;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.dictionary.min_match_length = 5;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);

    auto strict = limits;
    strict.max_entropy_table_entries = 4537;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(short_stream(),
                                                          strict),
              LzssShortMatchPreflightError::limit_exceeded);
}

TEST(LzssShortMatchPreflight, ComputesCompleteHandVectorRequirements) {
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_short_match_frame_semantics(
                  short_frame(), short_descriptor(), context, requirements),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(requirements.serialized_frame_bytes, 87U);
    EXPECT_EQ(requirements.token_count, 2U);
    EXPECT_EQ(requirements.raw_frame_bytes, 4U);
    constexpr auto model_bytes =
        sizeof(marc::entropy::internal::LzssShortMatchRangeDecoder);
    EXPECT_EQ(requirements.aggregate_working_bytes,
              87U + 2 * sizeof(marc::dictionary::internal::LzssTypedToken)
                  + 4U + model_bytes);
}

TEST(LzssShortMatchPreflight, RejectsCountsAndDescriptorBeforePublication) {
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    auto frame = short_frame();
    auto descriptor = short_descriptor();
    LzssShortMatchFrameRequirements requirements{777, 777, 777, 777};

    frame.event_count = 9;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    frame.decision_count = 37;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    frame.payload_size = 16;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    descriptor.context_count = 31;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::invalid_descriptor);
    EXPECT_EQ(requirements.serialized_frame_bytes, 777U);
    EXPECT_EQ(requirements.token_count, 777U);
    EXPECT_EQ(requirements.raw_frame_bytes, 777U);
    EXPECT_EQ(requirements.aggregate_working_bytes, 777U);
}

TEST(LzssShortMatchPreflight, RejectsSequenceSizeAndWorkspaceLimits) {
    const auto stream = short_stream();
    auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements requirements{777, 777, 777, 777};
    auto frame = short_frame();
    frame.sequence = 1;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::unexpected_sequence);
    frame = short_frame();
    frame.uncompressed_size = 3;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::unexpected_frame_size);

    limits.max_block_size = 4;
    constexpr auto model_bytes =
        sizeof(marc::entropy::internal::LzssShortMatchRangeDecoder);
    limits.max_internal_buffered_bytes =
        87 + 2 * sizeof(marc::dictionary::internal::LzssTypedToken)
        + 4 + model_bytes - 1;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  short_frame(), short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::limit_exceeded);
    EXPECT_EQ(requirements.aggregate_working_bytes, 777U);
}

TEST(LzssShortMatchPreflight, EnforcesMaximumFramePayloadCeiling) {
    auto stream = short_stream();
    stream.frame_size = 65536;
    stream.original_size = 65536;
    constexpr std::uint32_t raw = 65536;
    constexpr std::uint32_t decisions = 9 * raw;
    constexpr std::uint32_t maximum_payload = 18 * raw + 5;
    TypedContextFrameHeader frame{
        0, 0, raw, 21846, 43692, decisions, maximum_payload, 16, 0, 0};
    TypedContextRangeDescriptor descriptor{
        decisions, maximum_payload, 32};
    const auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, {stream, limits, 0, 0}, requirements),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(requirements.serialized_frame_bytes, 18U * raw + 85U);

    ++frame.payload_size;
    ++descriptor.payload_size;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, {stream, limits, 0, 0}, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
}

TEST(LzssShortMatchBytePreflight, ParsesOnlyExactReservedStreamHeader) {
    auto bytes = short_stream_bytes();
    const auto limits = marc::core::DecoderLimits{};
    TypedContextStreamHeader parsed{};
    std::size_t consumed = 777;
    EXPECT_EQ(parse_lzss_short_match_stream_header(
                  std::span<const std::byte>{bytes}.first(111), limits,
                  parsed, consumed),
              LzssShortMatchPreflightError::truncated_stream_header);
    EXPECT_EQ(consumed, 777U);
    ASSERT_EQ(parse_lzss_short_match_stream_header(bytes, limits, parsed,
                                                    consumed),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(consumed, 112U);
    EXPECT_EQ(parsed.dictionary.min_match_length, 3U);
    EXPECT_EQ(parsed.dictionary_variant, 7U);
    EXPECT_EQ(parsed.context_variant, 6U);

    bytes[52] = std::byte{1};
    consumed = 777;
    EXPECT_EQ(parse_lzss_short_match_stream_header(bytes, limits, parsed,
                                                    consumed),
              LzssShortMatchPreflightError::nonzero_reserved);
    EXPECT_EQ(consumed, 777U);
    bytes = short_stream_bytes();
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 14,
                                     std::uint16_t{6}));
    EXPECT_EQ(parse_lzss_short_match_stream_header(bytes, limits, parsed,
                                                    consumed),
              LzssShortMatchPreflightError::unsupported_format);
    bytes = short_stream_bytes();
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 68,
                                     std::uint32_t{5}));
    EXPECT_EQ(parse_lzss_short_match_stream_header(bytes, limits, parsed,
                                                    consumed),
              LzssShortMatchPreflightError::invalid_stream);
}

TEST(LzssShortMatchBytePreflight, ValidatesFrameBeforePublishingLayout) {
    auto bytes = short_frame_bytes();
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    TypedContextFrameLayout layout{};
    LzssShortMatchFrameRequirements requirements{};
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(
                  std::span<const std::byte>{bytes}.first(86), context,
                  layout, requirements),
              LzssShortMatchPreflightError::truncated_frame);
    ASSERT_EQ(preflight_lzss_short_match_frame_bytes(bytes, context, layout,
                                                     requirements),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(layout.serialized_size, 87U);
    EXPECT_EQ(requirements.serialized_frame_bytes, 87U);

    layout.serialized_size = 777;
    requirements.serialized_frame_bytes = 777;
    bytes[48] = std::byte{1};
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(bytes, context, layout,
                                                     requirements),
              LzssShortMatchPreflightError::nonzero_reserved);
    EXPECT_EQ(layout.serialized_size, 777U);
    EXPECT_EQ(requirements.serialized_frame_bytes, 777U);
    bytes = short_frame_bytes();
    bytes[76] = std::byte{1};
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(bytes, context, layout,
                                                     requirements),
              LzssShortMatchPreflightError::nonzero_reserved);
    bytes = short_frame_bytes();
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 72,
                                     std::uint16_t{31}));
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(bytes, context, layout,
                                                     requirements),
              LzssShortMatchPreflightError::invalid_descriptor);
}

TEST(LzssShortMatchBytePreflight, RejectsInvalidStreamFieldsAtomically) {
    const auto limits = marc::core::DecoderLimits{};
    auto bytes = short_stream_bytes();
    TypedContextStreamHeader parsed{};
    parsed.frame_size = 777;
    std::size_t consumed = 777;
    const auto expect_error = [&](const LzssShortMatchPreflightError expected) {
        EXPECT_EQ(parse_lzss_short_match_stream_header(bytes, limits, parsed,
                                                        consumed), expected);
        EXPECT_EQ(parsed.frame_size, 777U);
        EXPECT_EQ(consumed, 777U);
        bytes = short_stream_bytes();
    };
    bytes[0] = std::byte{0};
    expect_error(LzssShortMatchPreflightError::invalid_magic);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 6,
                                     std::uint16_t{1}));
    expect_error(LzssShortMatchPreflightError::unsupported_version);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 8,
                                     std::uint16_t{63}));
    expect_error(LzssShortMatchPreflightError::invalid_header_size);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 10,
                                     std::uint16_t{3}));
    expect_error(LzssShortMatchPreflightError::unsupported_feature);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 16,
                                     std::uint16_t{4}));
    expect_error(LzssShortMatchPreflightError::unsupported_format);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 28,
                                     std::uint32_t{1}));
    expect_error(LzssShortMatchPreflightError::invalid_stream);
    bytes[88] = std::byte{1};
    expect_error(LzssShortMatchPreflightError::nonzero_reserved);
    bytes[104] = std::byte{1};
    expect_error(LzssShortMatchPreflightError::nonzero_reserved);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 84,
                                     std::uint16_t{31}));
    expect_error(LzssShortMatchPreflightError::invalid_stream);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 76,
                                     std::uint32_t{1}));
    expect_error(LzssShortMatchPreflightError::invalid_stream);
}

TEST(LzssShortMatchBytePreflight, RejectsFrameTruncationAndFieldMutations) {
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    auto bytes = short_frame_bytes();
    TypedContextFrameLayout layout{};
    layout.serialized_size = 777;
    LzssShortMatchFrameRequirements requirements{777, 777, 777, 777};
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(
                  std::span<const std::byte>{bytes}.first(63), context,
                  layout, requirements),
              LzssShortMatchPreflightError::truncated_frame_header);
    EXPECT_EQ(preflight_lzss_short_match_frame_bytes(
                  std::span<const std::byte>{bytes}.first(79), context,
                  layout, requirements),
              LzssShortMatchPreflightError::truncated_descriptor);
    const auto expect_error = [&](const LzssShortMatchPreflightError expected) {
        EXPECT_EQ(preflight_lzss_short_match_frame_bytes(
                      bytes, context, layout, requirements), expected);
        EXPECT_EQ(layout.serialized_size, 777U);
        EXPECT_EQ(requirements.serialized_frame_bytes, 777U);
        bytes = short_frame_bytes();
    };
    bytes[0] = std::byte{0};
    expect_error(LzssShortMatchPreflightError::invalid_magic);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 4,
                                     std::uint16_t{63}));
    expect_error(LzssShortMatchPreflightError::invalid_header_size);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 8,
                                     std::uint64_t{1}));
    expect_error(LzssShortMatchPreflightError::unexpected_sequence);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 32,
                                     std::uint32_t{16}));
    expect_error(LzssShortMatchPreflightError::contradictory_counts);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 68,
                                     std::uint32_t{8}));
    expect_error(LzssShortMatchPreflightError::invalid_descriptor);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 74,
                                     std::uint16_t{1}));
    expect_error(LzssShortMatchPreflightError::unsupported_feature);
}

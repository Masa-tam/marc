#include "frame/lzss_contextual_rans_frame_decoder.hpp"

#include "context/lzss_contextual_rans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualRansFormatError;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] std::vector<std::byte> canonical_frame_vector() {
    std::vector<std::byte> bytes(98);
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x08};
    bytes[36] = std::byte{0x1a};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    std::ranges::copy(payload, bytes.begin() + 90);
    return bytes;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config() {
    LzssContextualRansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader four_mib_stream_config(
    const std::uint32_t raw_size) {
    auto stream = stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    stream.frequency_entry_count = 4566;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader sixteen_mib_stream_config(
    const std::uint32_t raw_size) {
    auto stream = stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    stream.frequency_entry_count = 4582;
    return stream;
}

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU};
}

} // namespace

TEST(LzssContextualRansFrameDecoder, DecodesSpecifiedFrameAtomically) {
    const auto frame = canonical_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 2> tokens{token_marker(), token_marker()};
    std::array raw{std::byte{0xcc}, std::byte{0xcc}};

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_table_entries,
              contextual_rans_decode_table_entries);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(result.token_decode.format_error,
              ContextualRansFormatError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xcc);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder,
     FourMiBIdentityDecodesFirstNewDistanceExactly) {
    constexpr std::uint32_t distance = 1'048'577;
    std::vector<LzssTypedToken> source_tokens;
    source_tokens.reserve(4'067);
    source_tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < 4'064; ++index) {
        source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 64});
    source_tokens.push_back(
        {LzssTypedTokenKind::match, 0, distance, 258});
    constexpr std::uint32_t raw_size = distance + 258;
    const auto stream = four_mib_stream_config(raw_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    constexpr auto variant = marc::context::internal::
        LzssFieldContextVariant::field_context_4m;
    marc::entropy::internal::ContextualRansDescriptor descriptor{};
    const auto entropy_plan = marc::context::internal::
        plan_lzss_contextual_rans_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            descriptor, variant);
    ASSERT_EQ(entropy_plan.error,
              marc::context::internal::
                  LzssContextualRansEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::context::internal::encode_lzss_contextual_rans_tokens(
                  source_tokens, stream.dictionary, token_context, limits,
                  payload, descriptor, variant).error,
              marc::context::internal::
                  LzssContextualRansEncodeError::none);
    std::vector<std::byte> serialized_descriptor(
        entropy_plan.descriptor_size);
    std::size_t descriptor_written{};
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, entropy_plan.decision_count,
                  static_cast<std::uint32_t>(payload.size()), limits,
                  serialized_descriptor, descriptor_written, variant),
              ContextualRansFormatError::none);
    ASSERT_EQ(descriptor_written, serialized_descriptor.size());

    const LzssContextualRansFrameHeader header{
        0,
        0,
        raw_size,
        static_cast<std::uint32_t>(source_tokens.size()),
        static_cast<std::uint32_t>(entropy_plan.event_count),
        entropy_plan.decision_count,
        static_cast<std::uint32_t>(payload.size()),
        static_cast<std::uint32_t>(serialized_descriptor.size()),
        0,
        0};
    std::vector<std::byte> frame(
        lzss_contextual_rans_frame_header_size
        + serialized_descriptor.size() + payload.size());
    ASSERT_EQ(serialize_lzss_contextual_rans_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte,
                            lzss_contextual_rans_frame_header_size>{
                      frame.data(), lzss_contextual_rans_frame_header_size}),
              LzssContextualRansFrameHeaderError::none);
    std::ranges::copy(
        serialized_descriptor,
        frame.begin() + lzss_contextual_rans_frame_header_size);
    std::ranges::copy(
        payload,
        frame.begin() + lzss_contextual_rans_frame_header_size
            + serialized_descriptor.size());

    auto table_storage = tables();
    std::vector<LzssTypedToken> decoded_tokens(source_tokens.size());
    std::vector<std::byte> raw(raw_size, std::byte{0xcc});
    auto failed = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage,
        std::span{decoded_tokens}.first(decoded_tokens.size() - 1), raw);
    EXPECT_EQ(failed.error,
              LzssContextualRansFrameDecodeError::token_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    failed = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage, decoded_tokens,
        std::span{raw}.first(raw.size() - 1));
    EXPECT_EQ(failed.error,
              LzssContextualRansFrameDecodeError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    const auto decoded = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, 258U);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssContextualRansFrameDecoder,
     SixteenMiBIdentityDecodesFirstNewDistanceExactly) {
    constexpr std::uint32_t distance = 4'194'305;
    constexpr std::size_t repeated_matches = 16'256;
    std::vector<LzssTypedToken> source_tokens;
    source_tokens.reserve(repeated_matches + 3);
    source_tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < repeated_matches; ++index) {
        source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 256});
    source_tokens.push_back(
        {LzssTypedTokenKind::match, 0, distance, 258});
    constexpr std::uint32_t raw_size = distance + 258;
    const auto stream = sixteen_mib_stream_config(raw_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    constexpr auto variant = marc::context::internal::
        LzssFieldContextVariant::field_context_16m;
    marc::entropy::internal::ContextualRansDescriptor descriptor{};
    const auto entropy_plan = marc::context::internal::
        plan_lzss_contextual_rans_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            descriptor, variant);
    ASSERT_EQ(entropy_plan.error,
              marc::context::internal::
                  LzssContextualRansEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::context::internal::encode_lzss_contextual_rans_tokens(
                  source_tokens, stream.dictionary, token_context, limits,
                  payload, descriptor, variant).error,
              marc::context::internal::
                  LzssContextualRansEncodeError::none);
    std::vector<std::byte> serialized_descriptor(
        entropy_plan.descriptor_size);
    std::size_t descriptor_written{};
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, entropy_plan.decision_count,
                  static_cast<std::uint32_t>(payload.size()), limits,
                  serialized_descriptor, descriptor_written, variant),
              ContextualRansFormatError::none);
    ASSERT_EQ(descriptor_written, serialized_descriptor.size());

    const LzssContextualRansFrameHeader header{
        0,
        0,
        raw_size,
        static_cast<std::uint32_t>(source_tokens.size()),
        static_cast<std::uint32_t>(entropy_plan.event_count),
        entropy_plan.decision_count,
        static_cast<std::uint32_t>(payload.size()),
        static_cast<std::uint32_t>(serialized_descriptor.size()),
        0,
        0};
    std::vector<std::byte> frame(
        lzss_contextual_rans_frame_header_size
        + serialized_descriptor.size() + payload.size());
    ASSERT_EQ(serialize_lzss_contextual_rans_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte,
                            lzss_contextual_rans_frame_header_size>{
                      frame.data(), lzss_contextual_rans_frame_header_size}),
              LzssContextualRansFrameHeaderError::none);
    std::ranges::copy(
        serialized_descriptor,
        frame.begin() + lzss_contextual_rans_frame_header_size);
    std::ranges::copy(
        payload,
        frame.begin() + lzss_contextual_rans_frame_header_size
            + serialized_descriptor.size());

    auto table_storage = tables();
    std::vector<LzssTypedToken> decoded_tokens(source_tokens.size());
    std::vector<std::byte> raw(raw_size, std::byte{0xcc});
    auto failed = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage,
        std::span{decoded_tokens}.first(decoded_tokens.size() - 1), raw);
    EXPECT_EQ(failed.error,
              LzssContextualRansFrameDecodeError::token_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    failed = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage, decoded_tokens,
        std::span{raw}.first(raw.size() - 1));
    EXPECT_EQ(failed.error,
              LzssContextualRansFrameDecodeError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    const auto decoded = decode_lzss_contextual_rans_frame(
        frame, {stream, limits, 0, 0}, table_storage, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, 258U);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssContextualRansFrameDecoder, ConsumesOnePreflightedFrame) {
    auto frame = canonical_frame_vector();
    const auto expected = frame.size();
    frame.push_back(std::byte{0xa5});
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, expected);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssContextualRansFrameDecoder,
     DescriptorAndTruncationFailuresPreserveOutput) {
    auto frame = canonical_frame_vector();
    frame[84] = std::byte{0x01};
    frame[85] = std::byte{0x00};
    frame[86] = std::byte{0x00};
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.descriptor_error,
              ContextualRansFormatError::noncanonical_representation);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});

    frame = canonical_frame_vector();
    result = decode_lzss_contextual_rans_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              LzssContextualRansFramePreflightError::truncated_frame);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder,
     RejectsDescriptorExtentsOutsideCanonicalBounds) {
    auto frame = canonical_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    frame[36] = std::byte{0x16};
    auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.header_error,
              LzssContextualRansFrameHeaderError::contradictory_counts);

    frame = canonical_frame_vector();
    frame[36] = std::byte{0x42};
    frame[37] = std::byte{0x23};
    result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.header_error,
              LzssContextualRansFrameHeaderError::contradictory_counts);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder, CapacityFailuresPrecedeWrites) {
    const auto frame = canonical_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::ranges::fill(table_storage, marker);
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0},
        std::span{table_storage}.first(table_storage.size() - 1), tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::table_output_too_small);
    result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage,
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::token_output_too_small);
    result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(table_storage, [](const auto& entry) {
        return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
            && entry.symbol == 0xa5;
    }));
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder,
     RejectsSerializedRawAliasingBeforeWrites) {
    auto frame = canonical_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    const auto before = frame[0];

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before);
}

#include "frame/lzss_contextual_blocked_huffman_frame_decoder.hpp"

#include "context/lzss_contextual_blocked_huffman_encoder.hpp"
#include "core/endian.hpp"

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
using marc::entropy::internal::ContextualBlockedHuffmanFormatError;
using marc::entropy::internal::HuffmanDecodeTable;

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader stream_config() {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader stream_config_4m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 22;
    stream.original_size = 5;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    return stream;
}

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader stream_config_16m(
    const std::uint32_t raw_size) {
    auto stream = stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    return stream;
}

[[nodiscard]] std::array<std::byte, 64> unchecked_frame_header(
    const LzssContextualBlockedHuffmanFrameHeader& header) {
    std::array<std::byte, 64> encoded{};
    constexpr std::array magic{
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x32}};
    std::ranges::copy(magic, encoded.begin());
    const std::span<std::byte> bytes{encoded};
    EXPECT_TRUE(marc::core::store_le(bytes, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(bytes, 6, header.flags));
    EXPECT_TRUE(marc::core::store_le(bytes, 8, header.sequence));
    EXPECT_TRUE(marc::core::store_le(bytes, 16, header.uncompressed_size));
    EXPECT_TRUE(marc::core::store_le(bytes, 20, header.token_count));
    EXPECT_TRUE(marc::core::store_le(bytes, 24, header.event_count));
    EXPECT_TRUE(marc::core::store_le(bytes, 28, header.decision_count));
    EXPECT_TRUE(marc::core::store_le(bytes, 32, header.payload_size));
    EXPECT_TRUE(marc::core::store_le(bytes, 36, header.descriptor_size));
    EXPECT_TRUE(
        marc::core::store_le(bytes, 40, header.context_side_data_size));
    EXPECT_TRUE(
        marc::core::store_le(bytes, 44, header.checksum_trailer_size));
    return encoded;
}

[[nodiscard]] constexpr std::array<std::byte, 24> descriptor_bytes() {
    return {
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{15}, std::byte{3}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{65}, std::byte{0}};
}

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> frame(88);
    const auto stream = stream_config();
    const LzssContextualBlockedHuffmanFrameHeader header{
        0, 0, 1, 1, 2, 2, 0, 24, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, 0, 0},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    std::ranges::copy(descriptor_bytes(), frame.begin() + 64);
    return frame;
}

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU};
}

} // namespace

TEST(LzssContextualBlockedHuffmanFrameFormat,
     SerializesAndParsesDocumentedStreamHeader) {
    std::array<std::byte, 112> encoded{};
    const auto stream = stream_config();
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  stream, {}, encoded),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    constexpr std::array prefix{
        std::byte{0x4D}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x40}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
        std::byte{0x40}};
    EXPECT_TRUE(std::ranges::equal(prefix, std::span{encoded}.first(21)));
    constexpr std::array dictionary{
        std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0},
        std::byte{5}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{1}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(dictionary,
                                  std::span{encoded}.subspan<64, 16>()));
    constexpr std::array entropy{
        std::byte{15}, std::byte{4}, std::byte{31}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(entropy,
                                  std::span{encoded}.subspan<80, 16>()));
    LzssContextualBlockedHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_blocked_huffman_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.frame_size, 64U);
    EXPECT_EQ(parsed.original_size, 1U);
}

TEST(LzssContextualBlockedHuffmanFrameFormat,
     SelectsOneMiBIdentityAndFrameCeilings) {
    auto selected = stream_config();
    selected.original_size = 5;
    selected.dictionary.window_size = 1'048'576;
    selected.dictionary_variant = 3;
    selected.context_variant = 2;
    std::array<std::byte, 112> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  selected, {}, encoded),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{3});
    EXPECT_EQ(encoded[15], std::byte{0});
    EXPECT_EQ(encoded[96], std::byte{1});
    EXPECT_EQ(encoded[98], std::byte{2});

    LzssContextualBlockedHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_blocked_huffman_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary_variant, 3U);
    EXPECT_EQ(parsed.context_algorithm, 1U);
    EXPECT_EQ(parsed.context_variant, 2U);
    EXPECT_EQ(parsed.dictionary.window_size, 1'048'576U);

    auto crossed = selected;
    crossed.dictionary_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  contradictory_parameters);
    crossed = selected;
    crossed.context_variant = 1;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  contradictory_parameters);
    crossed = selected;
    crossed.dictionary_variant = 99;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  unsupported_dictionary_variant);
    crossed = selected;
    crossed.context_algorithm = 99;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  unknown_context_model);
    crossed = selected;
    crossed.context_variant = 99;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  unsupported_context_variant);

    LzssContextualBlockedHuffmanFrameHeader header{
        0, 0, 5, 1, 2, 30, 1, 2579, 0, 0};
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {selected, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    auto legacy = selected;
    legacy.dictionary.window_size = 65536;
    legacy.dictionary_variant = 2;
    legacy.context_variant = 1;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {legacy, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);
}

TEST(LzssContextualBlockedHuffmanFrameFormat,
     RoundTripsFourMiBIdentityAndSelectsSevenFBound) {
    const auto stream = stream_config_4m();
    std::array<std::byte, 112> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  stream, {}, encoded),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{4});
    EXPECT_EQ(encoded[15], std::byte{0});
    EXPECT_EQ(encoded[16], std::byte{2});
    EXPECT_EQ(encoded[18], std::byte{2});
    EXPECT_EQ(encoded[98], std::byte{3});

    LzssContextualBlockedHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_blocked_huffman_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 22);
    EXPECT_EQ(parsed.dictionary_variant, 4U);
    EXPECT_EQ(parsed.context_variant, 3U);

    auto crossed = stream;
    crossed.context_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_stream_header(
                  crossed, {}),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  contradictory_parameters);

    LzssContextualBlockedHuffmanFrameHeader header{
        0, 0, 5, 2, 4, 35, 1,
        static_cast<std::uint32_t>(
            marc::entropy::internal::
                contextual_blocked_huffman_min_descriptor_size),
        0, 0};
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    auto one_mib = stream;
    one_mib.dictionary.window_size = UINT32_C(1) << 20;
    one_mib.dictionary_variant = 3;
    one_mib.context_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {one_mib, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);

    header.decision_count = 30;
    header.descriptor_size = static_cast<std::uint32_t>(
        marc::entropy::internal::
            contextual_blocked_huffman_max_descriptor_size_v3);
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {one_mib, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);
}

TEST(LzssContextualBlockedHuffmanFrameFormat,
     SixteenMiBAdmissionSelectsExactBounds) {
    const auto stream = stream_config_16m(5);
    LzssContextualBlockedHuffmanFrameHeader header{
        0, 0, 5, 2, 4, 35, 66,
        static_cast<std::uint32_t>(
            marc::entropy::internal::
                contextual_blocked_huffman_max_descriptor_size_v4),
        0, 0};
    EXPECT_EQ(validate_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);

    auto encoded = unchecked_frame_header(header);
    LzssContextualBlockedHuffmanFrameHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_blocked_huffman_frame_header(
                  encoded, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.decision_count, 35U);
    EXPECT_EQ(parsed.descriptor_size, 2597U);

    auto invalid = header;
    invalid.decision_count = 36;
    encoded = unchecked_frame_header(invalid);
    parsed.sequence = UINT64_C(0xcccccccccccccccc);
    consumed = 0xcccc;
    EXPECT_EQ(parse_lzss_contextual_blocked_huffman_frame_header(
                  encoded, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);
    EXPECT_EQ(parsed.sequence, UINT64_C(0xcccccccccccccccc));
    EXPECT_EQ(consumed, 0xccccU);
    invalid = header;
    invalid.payload_size = 67;
    encoded = unchecked_frame_header(invalid);
    parsed.sequence = UINT64_C(0xcccccccccccccccc);
    consumed = 0xcccc;
    EXPECT_EQ(parse_lzss_contextual_blocked_huffman_frame_header(
                  encoded, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);
    EXPECT_EQ(parsed.sequence, UINT64_C(0xcccccccccccccccc));
    EXPECT_EQ(consumed, 0xccccU);
    invalid = header;
    invalid.descriptor_size = 2598;
    encoded = unchecked_frame_header(invalid);
    parsed.sequence = UINT64_C(0xcccccccccccccccc);
    consumed = 0xcccc;
    EXPECT_EQ(parse_lzss_contextual_blocked_huffman_frame_header(
                  encoded, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualBlockedHuffmanFrameHeaderError::
                  contradictory_counts);
    EXPECT_EQ(parsed.sequence, UINT64_C(0xcccccccccccccccc));
    EXPECT_EQ(consumed, 0xccccU);
}

TEST(LzssContextualBlockedHuffmanFrameFormat,
     RejectsStreamIdentityAndParametersAtomically) {
    std::array<std::byte, 112> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  stream_config(), {}, encoded),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    LzssContextualBlockedHuffmanStreamHeader parsed{};
    parsed.frame_size = 0xCCCCCCCCU;
    std::size_t consumed = 0xCCCCU;
    encoded[16] = std::byte{3};
    EXPECT_EQ(parse_lzss_contextual_blocked_huffman_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  unknown_entropy_algorithm);
    EXPECT_EQ(parsed.frame_size, 0xCCCCCCCCU);
    EXPECT_EQ(consumed, 0xCCCCU);
    encoded[16] = std::byte{2};
    encoded[80] = std::byte{14};
    EXPECT_EQ(parse_lzss_contextual_blocked_huffman_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualBlockedHuffmanStreamHeaderError::
                  invalid_entropy_parameters);
}

TEST(LzssContextualBlockedHuffmanFrameFormat,
     PreflightsDocumentedFrameAndRejectsMalformedExtent) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    LzssContextualBlockedHuffmanFrameLayout layout{};
    auto result = preflight_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, layout);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, 88U);
    EXPECT_EQ(layout.header.descriptor_size, 24U);
    EXPECT_EQ(layout.descriptor.field_models[1].single_symbol, 65U);

    frame[84] = std::byte{1};
    result = preflight_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFramePreflightError::
                  descriptor_error);
    EXPECT_EQ(result.descriptor_error,
              ContextualBlockedHuffmanFormatError::noncanonical_representation);
    frame = frame_vector();
    result = preflight_lzss_contextual_blocked_huffman_frame(
        std::span<const std::byte>{frame}.first(87),
        {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFramePreflightError::
                  truncated_frame);
}

TEST(LzssContextualBlockedHuffmanFrameDecoder,
     DecodesSpecifiedFrameAtomically) {
    auto frame = frame_vector();
    frame.push_back(std::byte{0xA5});
    const auto stream = stream_config();
    std::array<LzssTypedToken, 2> tokens{token_marker(), token_marker()};
    std::array raw{std::byte{0xCC}, std::byte{0xCC}};
    const auto result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, tokens, raw);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, 88U);
    EXPECT_EQ(result.required_table_entries, 0U);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xCC);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xCC});
}

TEST(LzssContextualBlockedHuffmanFrameDecoder,
     SixteenMiBIdentityDecodesFirstNewDistanceAtomically) {
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
    const auto stream = stream_config_16m(raw_size);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    constexpr auto variant = marc::context::internal::
        LzssFieldContextVariant::field_context_16m;
    marc::entropy::internal::ContextualBlockedHuffmanDescriptor descriptor{};
    const auto entropy_plan = marc::context::internal::
        plan_lzss_contextual_blocked_huffman_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            descriptor, variant);
    ASSERT_EQ(entropy_plan.error,
              marc::context::internal::
                  LzssContextualBlockedHuffmanEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::context::internal::
                  encode_lzss_contextual_blocked_huffman_tokens(
                      source_tokens, stream.dictionary, token_context, limits,
                      payload, descriptor, variant).error,
              marc::context::internal::
                  LzssContextualBlockedHuffmanEncodeError::none);
    std::vector<std::byte> serialized_descriptor(
        entropy_plan.descriptor_size);
    std::size_t descriptor_written{};
    ASSERT_EQ(marc::entropy::internal::
                  serialize_contextual_blocked_huffman_descriptor(
                      descriptor, entropy_plan.decision_count,
                      static_cast<std::uint32_t>(payload.size()), limits,
                      serialized_descriptor, descriptor_written, variant),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(descriptor_written, serialized_descriptor.size());

    const LzssContextualBlockedHuffmanFrameHeader header{
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
    const auto encoded_header = unchecked_frame_header(header);
    std::vector<std::byte> frame(
        encoded_header.size() + serialized_descriptor.size()
        + payload.size());
    std::ranges::copy(encoded_header, frame.begin());
    std::ranges::copy(
        serialized_descriptor, frame.begin() + encoded_header.size());
    std::ranges::copy(
        payload,
        frame.begin() + encoded_header.size() + serialized_descriptor.size());

    std::array<HuffmanDecodeTable,
               marc::entropy::internal::
                   contextual_blocked_huffman_max_table_count>
        tables{};
    std::vector<LzssTypedToken> decoded_tokens(
        source_tokens.size(), token_marker());
    std::vector<std::byte> raw(raw_size, std::byte{0xcc});
    auto crossed_stream = stream;
    crossed_stream.dictionary.window_size = UINT32_C(1) << 22;
    crossed_stream.dictionary_variant = 4;
    crossed_stream.context_variant = 3;
    const auto crossed = decode_lzss_contextual_blocked_huffman_frame(
        frame, {crossed_stream, limits, 0, 0}, tables, decoded_tokens, raw);
    EXPECT_NE(crossed.error,
              LzssContextualBlockedHuffmanFrameDecodeError::none);
    EXPECT_TRUE(std::ranges::all_of(
        decoded_tokens, [](const auto& token) {
            return token.literal == 0xcc && token.distance == 0xccccccccU;
        }));
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    const auto decoded = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, limits, 0, 0}, tables, decoded_tokens, raw);
    ASSERT_EQ(decoded.error,
              LzssContextualBlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, 258U);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssContextualBlockedHuffmanFrameDecoder,
     FailuresPreserveRawAndConsumedExtent) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xCC}};
    auto result = decode_lzss_contextual_blocked_huffman_frame(
        std::span<const std::byte>{frame}.first(87),
        {stream, {}, 0, 0}, {}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::preflight_error);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    frame[8] = std::byte{1};
    result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.header_error,
              LzssContextualBlockedHuffmanFrameHeaderError::
                  unexpected_sequence);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssContextualBlockedHuffmanFrameDecoder,
     RejectsCapacityAndWorkspaceAliasingBeforeWrites) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xCC}};
    auto result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, {}, raw);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::
                  token_output_too_small);
    result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, tokens, {});
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::
                  raw_output_too_small);
    result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::
                  overlapping_workspaces);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

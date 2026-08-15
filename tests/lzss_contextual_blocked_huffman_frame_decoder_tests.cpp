#include "frame/lzss_contextual_blocked_huffman_frame_decoder.hpp"

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

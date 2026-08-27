#include "frame/lzss_contextual_adaptive_huffman_frame_decoder.hpp"
#include "context/lzss_contextual_adaptive_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::ContextualAdaptiveHuffmanDecodeError;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries_v4;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries_v4;

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

struct SixteenMiBWorkspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries_v4);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries_v4);
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config() {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config_16m(
    const std::uint32_t raw_size) {
    auto stream = stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> frame(82);
    const auto stream = stream_config();
    const LzssContextualAdaptiveHuffmanFrameHeader header{
        0, 0, 1, 1, 2, 2, 2, 16, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, frame.begin() + 64);
    frame[80] = std::byte{0x82};
    frame[81] = std::byte{0x00};
    return frame;
}

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU};
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     DecodesSpecifiedFrameAtomically) {
    auto frame = frame_vector();
    frame.push_back(std::byte{0xA5});
    const auto stream = stream_config();
    Workspace workspace{};
    std::array<LzssTypedToken, 2> tokens{token_marker(), token_marker()};
    std::array raw{std::byte{0xCC}, std::byte{0xCC}};
    const auto result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols,
        tokens, raw);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, 82U);
    EXPECT_EQ(result.required_node_entries,
              contextual_adaptive_huffman_node_entries);
    EXPECT_EQ(result.required_symbol_entries,
              contextual_adaptive_huffman_symbol_entries);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xCC);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xCC});
}

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     SixteenMiBIdentityDecodesFirstNewDistanceAtomically) {
    constexpr std::uint32_t distance = UINT32_C(4194305);
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
    limits.max_lz_distance = UINT32_C(1) << 24;
    limits.max_entropy_table_entries =
        contextual_adaptive_huffman_node_entries_v4
        + contextual_adaptive_huffman_symbol_entries_v4;
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    constexpr auto variant = marc::context::internal::
        LzssFieldContextVariant::field_context_16m;
    marc::entropy::internal::ContextualAdaptiveHuffmanDescriptor descriptor{};
    SixteenMiBWorkspace workspace{};
    const auto entropy_plan = marc::context::internal::
        plan_lzss_contextual_adaptive_huffman_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            workspace.nodes, workspace.symbols, descriptor, variant);
    ASSERT_EQ(entropy_plan.error,
              marc::context::internal::
                  LzssContextualAdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::context::internal::
                  encode_lzss_contextual_adaptive_huffman_tokens(
                      source_tokens, stream.dictionary, token_context, limits,
                      workspace.nodes, workspace.symbols, payload, descriptor,
                      variant).error,
              marc::context::internal::
                  LzssContextualAdaptiveHuffmanEncodeError::none);
    std::array<std::byte,
               marc::entropy::internal::
                   contextual_adaptive_huffman_descriptor_size>
        serialized_descriptor{};
    ASSERT_EQ(marc::entropy::internal::
                  serialize_contextual_adaptive_huffman_descriptor(
                      descriptor, entropy_plan.decision_count,
                      static_cast<std::uint32_t>(payload.size()), limits,
                      serialized_descriptor),
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanFormatError::none);

    const LzssContextualAdaptiveHuffmanFrameHeader header{
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
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_frame_header_size>
        serialized_header{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, limits, 0, 0}, serialized_header),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    std::vector<std::byte> frame(
        serialized_header.size() + serialized_descriptor.size()
        + payload.size());
    std::ranges::copy(serialized_header, frame.begin());
    std::ranges::copy(
        serialized_descriptor, frame.begin() + serialized_header.size());
    std::ranges::copy(
        payload,
        frame.begin() + serialized_header.size()
            + serialized_descriptor.size());

    std::vector<LzssTypedToken> decoded_tokens(
        source_tokens.size(), token_marker());
    std::vector<std::byte> raw(raw_size, std::byte{0xcc});
    auto crossed_stream = stream;
    crossed_stream.dictionary.window_size = UINT32_C(1) << 22;
    crossed_stream.dictionary_variant = 4;
    crossed_stream.context_variant = 3;
    const auto crossed = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {crossed_stream, limits, 0, 0}, workspace.nodes,
        workspace.symbols, decoded_tokens, raw);
    EXPECT_NE(crossed.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_TRUE(std::ranges::all_of(
        decoded_tokens, [](const auto& token) {
            return token.literal == 0xcc && token.distance == 0xccccccccU;
        }));
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    const auto decoded = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, limits, 0, 0}, workspace.nodes, workspace.symbols,
        decoded_tokens, raw);
    ASSERT_EQ(decoded.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded.required_node_entries,
              contextual_adaptive_huffman_node_entries_v4);
    EXPECT_EQ(decoded.required_symbol_entries,
              contextual_adaptive_huffman_symbol_entries_v4);
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, 258U);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     PreflightFailuresPreserveOutputsAndConsumption) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    Workspace workspace{};
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xCC}};
    const auto result = decode_lzss_contextual_adaptive_huffman_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              LzssContextualAdaptiveHuffmanFramePreflightError::
                  truncated_frame);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     CapacityFailuresPrecedeAllWrites) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    Workspace workspace{};
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xCC}};
    auto result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0},
        std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
        workspace.symbols, tokens, raw);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                node_workspace_too_small);
    result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes,
        std::span{workspace.symbols}.first(workspace.symbols.size() - 1),
        tokens, raw);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                symbol_workspace_too_small);
    result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, {}, raw);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                token_output_too_small);
    result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, tokens,
        {});
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                raw_output_too_small);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     RejectsSerializedAndModelRawAliasingBeforeWrites) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    Workspace workspace{};
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    auto result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                overlapping_workspaces);

    auto node_bytes = std::as_writable_bytes(std::span{workspace.nodes});
    result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, tokens,
        node_bytes.first(1));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                overlapping_workspaces);
}

TEST(LzssContextualAdaptiveHuffmanFrameDecoder,
     EntropyFailurePreservesTokenAndRawOutput) {
    auto frame = frame_vector();
    frame[81] = std::byte{0x80};
    const auto stream = stream_config();
    Workspace workspace{};
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xCC}};
    const auto result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, workspace.nodes, workspace.symbols, tokens,
        raw);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameDecodeError::
                                token_decode_error);
    EXPECT_EQ(result.token_decode.entropy.error,
              ContextualAdaptiveHuffmanDecodeError::nonzero_padding);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

#include "frame/lzss_contextual_adaptive_huffman_frame_decoder.hpp"

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
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config() {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
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

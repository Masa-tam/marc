#include "context/lzss_contextual_adaptive_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::ContextualAdaptiveHuffmanDecodeError;
using marc::entropy::internal::ContextualAdaptiveHuffmanDescriptor;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;

struct Workspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries>
        symbols{};
};

[[nodiscard]] constexpr LzssTypedToken sentinel_token() {
    return {LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU};
}

void expect_token_eq(
    const LzssTypedToken& actual, const LzssTypedToken& expected) {
    EXPECT_EQ(actual.kind, expected.kind);
    EXPECT_EQ(actual.literal, expected.literal);
    EXPECT_EQ(actual.distance, expected.distance);
    EXPECT_EQ(actual.length, expected.length);
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext match_context() {
    return {2, 6, 6, 7, 0};
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     ValidatesAndDecodesDocumentedLiteral) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    Workspace workspace{};
    const auto validated =
        validate_lzss_contextual_adaptive_huffman_tokens(
            descriptor, payload, {}, literal_context(), {}, workspace.nodes,
            workspace.symbols);
    ASSERT_EQ(validated.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(validated.token_count, 1U);
    EXPECT_EQ(validated.raw_size, 1U);
    EXPECT_EQ(validated.entropy.bits_consumed, 9U);

    std::array<LzssTypedToken, 2> tokens{sentinel_token(), sentinel_token()};
    const auto decoded = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {}, workspace.nodes,
        workspace.symbols, tokens);
    ASSERT_EQ(decoded.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    expect_token_eq(tokens[0], {LzssTypedTokenKind::literal, 'A', 0, 0});
    expect_token_eq(tokens[1], sentinel_token());
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     DecodesLiteralThenOverlappingMatchWithBypass) {
    constexpr std::array payload{
        std::byte{0x82}, std::byte{0x06}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{6, 3, 31, 3, 0};
    Workspace workspace{};
    std::array<LzssTypedToken, 2> tokens{sentinel_token(), sentinel_token()};
    const auto decoded = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, match_context(), {}, workspace.nodes,
        workspace.symbols, tokens);
    ASSERT_EQ(decoded.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(decoded.token_count, 2U);
    EXPECT_EQ(decoded.raw_size, 7U);
    EXPECT_EQ(decoded.entropy.event_count, 6U);
    EXPECT_EQ(decoded.entropy.decision_count, 6U);
    EXPECT_EQ(decoded.entropy.bits_consumed, 19U);
    expect_token_eq(tokens[0], {LzssTypedTokenKind::literal, 'A', 0, 0});
    expect_token_eq(tokens[1], {LzssTypedTokenKind::match, 0, 1, 6});
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     FirstPassFailureLeavesTokenOutputUntouched) {
    constexpr std::array invalid_distance{
        std::byte{0x82}, std::byte{0x22}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{6, 3, 31, 3, 0};
    Workspace workspace{};
    std::array<LzssTypedToken, 2> tokens{sentinel_token(), sentinel_token()};
    const auto before = tokens;
    const auto result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, invalid_distance, {}, {2, 6, 6, 6, 0}, {},
        workspace.nodes, workspace.symbols, tokens);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_token);
    expect_token_eq(tokens[0], before[0]);
    expect_token_eq(tokens[1], before[1]);
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     RejectsCountsRawExtentAndTruncation) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    Workspace workspace{};
    auto result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, {1, 2, 3, 1, 0}, {}, workspace.nodes,
        workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_counts);

    result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, {1, 2, 2, 2, 0}, {}, workspace.nodes,
        workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::raw_size_mismatch);

    constexpr std::array truncated{std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor short_descriptor{2, 1, 31, 1, 0};
    result = validate_lzss_contextual_adaptive_huffman_tokens(
        short_descriptor, truncated, {}, literal_context(), {},
        workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::entropy_error);
    EXPECT_EQ(result.entropy.error,
              ContextualAdaptiveHuffmanDecodeError::truncated_bits);
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     RejectsShortWorkspacesAndTokenOutputAtomically) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    Workspace workspace{};
    auto result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {},
        std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
        workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanDecodeError::
                                node_workspace_too_small);
    result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {}, workspace.nodes,
        std::span{workspace.symbols}.first(workspace.symbols.size() - 1));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanDecodeError::
                                symbol_workspace_too_small);

    std::array<LzssTypedToken, 0> tokens{};
    result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {}, workspace.nodes,
        workspace.symbols, tokens);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanDecodeError::
                                token_output_too_small);
}

TEST(LzssContextualAdaptiveHuffmanDecoder, RejectsEveryTokenAlias) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    Workspace workspace{};
    std::array<LzssTypedToken, 1> token_storage{sentinel_token()};
    auto token_bytes = std::as_writable_bytes(std::span{token_storage});
    token_bytes[0] = payload[0];
    token_bytes[1] = payload[1];
    auto result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, std::span<const std::byte>{token_bytes}.first(2), {},
        literal_context(), {}, workspace.nodes, workspace.symbols,
        token_storage);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::overlapping_buffers);

    auto* node_tokens = reinterpret_cast<LzssTypedToken*>(
        workspace.nodes.data());
    result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {}, workspace.nodes,
        workspace.symbols, std::span<LzssTypedToken>{node_tokens, 1});
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::overlapping_buffers);

    alignas(LzssTypedToken)
        std::array<std::uint16_t,
                   contextual_adaptive_huffman_symbol_entries>
            aligned_symbols{};
    auto* symbol_tokens = reinterpret_cast<LzssTypedToken*>(
        aligned_symbols.data());
    result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), {}, workspace.nodes,
        aligned_symbols, std::span<LzssTypedToken>{symbol_tokens, 1});
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::overlapping_buffers);
}

TEST(LzssContextualAdaptiveHuffmanDecoder,
     EnforcesParametersAggregateAndTotalOutputLimits) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    Workspace workspace{};
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    auto result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, parameters, literal_context(), {},
        workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_parameters);

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes =
        contextual_adaptive_huffman_node_entries * sizeof(AdaptiveHuffmanNode)
        + contextual_adaptive_huffman_symbol_entries * sizeof(std::uint16_t)
        + sizeof(LzssTypedToken) + payload.size() - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, literal_context(), limits, workspace.nodes,
        workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, {}, context, limits, workspace.nodes,
        workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded);
}

} // namespace

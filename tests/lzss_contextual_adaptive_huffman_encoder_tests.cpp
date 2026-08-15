#include "context/lzss_contextual_adaptive_huffman_encoder.hpp"

#include "context/lzss_contextual_adaptive_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedFrameValidationContext;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::ContextualAdaptiveHuffmanDescriptor;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

constexpr LzssTypedToken literal_a{
    LzssTypedTokenKind::literal, 'A', 0, 0};
constexpr LzssTypedToken match_1_6{
    LzssTypedTokenKind::match, 0, 1, 6};

[[nodiscard]] constexpr LzssTypedFrameValidationContext frame_context(
    const std::uint32_t tokens, const std::uint32_t raw,
    const std::uint64_t committed = 0) {
    return {tokens, raw, committed};
}

void expect_token_eq(
    const LzssTypedToken& actual, const LzssTypedToken& expected) {
    EXPECT_EQ(actual.kind, expected.kind);
    EXPECT_EQ(actual.literal, expected.literal);
    EXPECT_EQ(actual.distance, expected.distance);
    EXPECT_EQ(actual.length, expected.length);
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanEncoder,
     PlansAndEncodesDocumentedLiteralDirectly) {
    constexpr std::array tokens{literal_a};
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), {}, workspace.nodes,
        workspace.symbols, descriptor);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_bits, 9U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(descriptor.final_valid_bits, 1U);

    std::array payload{
        std::byte{0xCC}, std::byte{0xCC}, std::byte{0xCC}};
    descriptor = {99, 99, 99, 99, 99};
    result = encode_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), {}, workspace.nodes,
        workspace.symbols, payload, descriptor);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(payload[0], std::byte{0x82});
    EXPECT_EQ(payload[1], std::byte{0x00});
    EXPECT_EQ(payload[2], std::byte{0xCC});
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     EncodesDocumentedMatchAndRoundTripsTokens) {
    constexpr std::array tokens{literal_a, match_1_6};
    Workspace encode_workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    std::array payload{
        std::byte{0xCC}, std::byte{0xCC}, std::byte{0xCC}, std::byte{0xCC}};
    const auto encoded = encode_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(2, 7), {}, encode_workspace.nodes,
        encode_workspace.symbols, payload, descriptor);
    ASSERT_EQ(encoded.error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(encoded.event_count, 6U);
    EXPECT_EQ(encoded.decision_count, 6U);
    EXPECT_EQ(encoded.payload_bits, 19U);
    EXPECT_EQ(encoded.payload_size, 3U);
    EXPECT_EQ(payload[0], std::byte{0x82});
    EXPECT_EQ(payload[1], std::byte{0x06});
    EXPECT_EQ(payload[2], std::byte{0x00});
    EXPECT_EQ(payload[3], std::byte{0xCC});

    Workspace decode_workspace{};
    std::array<LzssTypedToken, 2> decoded{};
    const auto result = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, std::span<const std::byte>{payload}.first(3), {},
        {2, 6, 6, 7, 0}, {}, decode_workspace.nodes,
        decode_workspace.symbols, decoded);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    expect_token_eq(decoded[0], literal_a);
    expect_token_eq(decoded[1], match_1_6);
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     MatchesOperationBoundaryWithoutOperationWorkspace) {
    constexpr std::array tokens{literal_a, match_1_6};
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 'A', 0},
        ModeledOperation{ModeledOperationKind::symbol, 1, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 21, 8, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 1},
        ModeledOperation{ModeledOperationKind::symbol, 24, 17, 0, 0}};
    Workspace token_workspace{};
    Workspace operation_workspace{};
    std::array<std::byte, 3> token_payload{};
    std::array<std::byte, 3> operation_payload{};
    ContextualAdaptiveHuffmanDescriptor token_descriptor{};
    ContextualAdaptiveHuffmanDescriptor operation_descriptor{};
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  tokens, {}, frame_context(2, 7), {}, token_workspace.nodes,
                  token_workspace.symbols, token_payload,
                  token_descriptor).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_adaptive_huffman_operations(
                      operations, {}, operation_workspace.nodes,
                      operation_workspace.symbols, operation_payload,
                      operation_descriptor,
                      marc::context::internal::LzssFieldContextVariant::
                          field_context_64k).error,
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(token_payload, operation_payload);
    EXPECT_EQ(token_descriptor.decision_count,
              operation_descriptor.decision_count);
    EXPECT_EQ(token_descriptor.final_valid_bits,
              operation_descriptor.final_valid_bits);
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     TokenAndCapacityFailuresPreserveOutputs) {
    constexpr std::array tokens{literal_a, match_1_6};
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    std::array payload{
        std::byte{0xCC}, std::byte{0xCC}, std::byte{0xCC}};
    auto result = encode_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(2, 7), {}, workspace.nodes,
        workspace.symbols, std::span{payload}.first(2), descriptor);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanEncodeError::
                                payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xCC};
    }));
    EXPECT_EQ(descriptor.decision_count, 99U);

    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.min_match_length = 4;
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, parameters, frame_context(2, 7), {}, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::invalid_parameters);

    constexpr std::array invalid{match_1_6};
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        invalid, {}, frame_context(1, 6), {}, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::invalid_token);

    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(2, 8), {}, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::invalid_token);
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     RejectsShortModelStorageAndAllTokenAliases) {
    constexpr std::array tokens{literal_a};
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    auto result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), {},
        std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanEncodeError::
                                node_workspace_too_small);
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), {}, workspace.nodes,
        std::span{workspace.symbols}.first(workspace.symbols.size() - 1),
        descriptor);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanEncodeError::
                                symbol_workspace_too_small);

    Workspace token_model_workspace{};
    auto* overlapping_token = reinterpret_cast<LzssTypedToken*>(
        token_model_workspace.nodes.data());
    *overlapping_token = literal_a;
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        std::span<const LzssTypedToken>{overlapping_token, 1}, {},
        frame_context(1, 1), {}, token_model_workspace.nodes,
        token_model_workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::overlapping_buffers);

    std::array token_output_storage{literal_a};
    auto token_output_bytes =
        std::as_writable_bytes(std::span{token_output_storage});
    Workspace token_output_workspace{};
    result = encode_lzss_contextual_adaptive_huffman_tokens(
        token_output_storage, {}, frame_context(1, 1), {},
        token_output_workspace.nodes, token_output_workspace.symbols,
        token_output_bytes.first(2), descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::overlapping_buffers);

    Workspace model_output_workspace{};
    auto node_bytes = std::as_writable_bytes(
        std::span{model_output_workspace.nodes});
    result = encode_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), {}, model_output_workspace.nodes,
        model_output_workspace.symbols, node_bytes.first(2), descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::overlapping_buffers);
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     EnforcesEntropyPayloadAggregateAndTotalOutputLimits) {
    constexpr std::array tokens{literal_a};
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_adaptive_huffman_node_entries
        + contextual_adaptive_huffman_symbol_entries - 1;
    auto result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), limits, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 1;
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), limits, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits = {};
    const auto aggregate = sizeof(LzssTypedToken)
        + workspace.nodes.size() * sizeof(AdaptiveHuffmanNode)
        + workspace.symbols.size() * sizeof(std::uint16_t) + 2U;
    limits.max_block_size = aggregate - 1;
    limits.max_internal_buffered_bytes = aggregate - 1;
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1), limits, workspace.nodes,
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits = {};
    result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, {}, frame_context(1, 1, limits.max_total_output_size), limits,
        workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 99U);
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     ProducesIdenticalBytesWithFreshWorkspaces) {
    constexpr std::array tokens{literal_a, match_1_6};
    Workspace first_workspace{};
    Workspace second_workspace{};
    std::array<std::byte, 3> first{};
    std::array<std::byte, 3> second{};
    ContextualAdaptiveHuffmanDescriptor first_descriptor{};
    ContextualAdaptiveHuffmanDescriptor second_descriptor{};
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  tokens, {}, frame_context(2, 7), {}, first_workspace.nodes,
                  first_workspace.symbols, first, first_descriptor).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  tokens, {}, frame_context(2, 7), {}, second_workspace.nodes,
                  second_workspace.symbols, second, second_descriptor).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first_descriptor.decision_count,
              second_descriptor.decision_count);
    EXPECT_EQ(first_descriptor.final_valid_bits,
              second_descriptor.final_valid_bits);
}

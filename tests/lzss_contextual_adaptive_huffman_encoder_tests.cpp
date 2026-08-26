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
using marc::entropy::internal::contextual_adaptive_huffman_node_entries_v2;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries_v3;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries_v4;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries_v2;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries_v3;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries_v4;

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

struct SelectedWorkspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries_v2);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries_v2);
};

struct FourMiBWorkspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries_v3);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries_v3);
};

struct SixteenMiBWorkspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries_v4);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries_v4);
};

[[nodiscard]] std::vector<LzssTypedToken> maximum_distance_tokens() {
    std::vector<LzssTypedToken> tokens;
    tokens.reserve(4067);
    tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < 4064; ++index) {
        tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    tokens.push_back({LzssTypedTokenKind::match, 0, 1, 63});
    tokens.push_back(
        {LzssTypedTokenKind::match, 0, 1'048'576, 5});
    return tokens;
}

[[nodiscard]] std::vector<LzssTypedToken> four_mib_distance_tokens() {
    std::vector<LzssTypedToken> tokens;
    tokens.reserve(16'259);
    tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < 16'256; ++index) {
        tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    tokens.push_back({LzssTypedTokenKind::match, 0, 1, 255});
    tokens.push_back(
        {LzssTypedTokenKind::match, 0, 4'194'304, 5});
    return tokens;
}

[[nodiscard]] std::vector<LzssTypedToken> sixteen_mib_distance_tokens() {
    std::vector<LzssTypedToken> tokens;
    tokens.reserve(65'030);
    tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < 65'027; ++index) {
        tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    tokens.push_back({LzssTypedTokenKind::match, 0, 1, 249});
    tokens.push_back(
        {LzssTypedTokenKind::match, 0, UINT32_C(16777216), 5});
    return tokens;
}

void expect_descriptor_eq(
    const ContextualAdaptiveHuffmanDescriptor& actual,
    const ContextualAdaptiveHuffmanDescriptor& expected) {
    EXPECT_EQ(actual.decision_count, expected.decision_count);
    EXPECT_EQ(actual.payload_size, expected.payload_size);
    EXPECT_EQ(actual.context_count, expected.context_count);
    EXPECT_EQ(actual.final_valid_bits, expected.final_valid_bits);
    EXPECT_EQ(actual.flags, expected.flags);
}

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

TEST(LzssContextualAdaptiveHuffmanEncoder,
     SelectedLayoutMatchesOperationsAndDecodesMaximumDistance) {
    const auto expected = maximum_distance_tokens();
    constexpr std::uint32_t prefix_size = 1'048'576;
    constexpr std::uint32_t raw_size = prefix_size + 5;
    constexpr auto variant = LzssFieldContextVariant::field_context_1m;
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = prefix_size;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = raw_size;
    limits.max_block_size = raw_size;
    limits.max_lz_distance = prefix_size;
    const LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(expected.size()), raw_size, 0};

    const auto operation_plan = plan_lzss_field_context_operations(
        expected, parameters, context, limits, variant);
    ASSERT_EQ(operation_plan.error, LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(operation_plan.operation_count);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  expected, parameters, context, limits, operations,
                  variant).error,
              LzssFieldContextError::none);
    ASSERT_GE(operations.size(), 2U);
    EXPECT_EQ(operations[operations.size() - 2].alphabet_size, 21U);
    EXPECT_EQ(operations[operations.size() - 2].value, 20U);
    EXPECT_EQ(operations.back().kind, ModeledOperationKind::bypass_bits);
    EXPECT_EQ(operations.back().bit_count, 20U);
    EXPECT_EQ(operations.back().value, 0U);

    SelectedWorkspace reference_workspace{};
    ContextualAdaptiveHuffmanDescriptor reference_descriptor{};
    const auto reference_plan = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, limits, reference_workspace.nodes,
            reference_workspace.symbols, reference_descriptor, variant);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_adaptive_huffman_operations(
                      operations, limits, reference_workspace.nodes,
                      reference_workspace.symbols, reference_payload,
                      reference_descriptor, variant).error,
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanEncodeError::none);

    SelectedWorkspace direct_workspace{};
    ContextualAdaptiveHuffmanDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_adaptive_huffman_tokens(
        expected, parameters, context, limits, direct_workspace.nodes,
        direct_workspace.symbols, direct_descriptor, variant);
    ASSERT_EQ(direct_plan.error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_bits, reference_plan.payload_bits);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());
    expect_descriptor_eq(direct_descriptor, reference_descriptor);

    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  direct_workspace.nodes, direct_workspace.symbols,
                  direct_payload, direct_descriptor, variant).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(direct_payload, reference_payload);

    SelectedWorkspace decode_workspace{};
    std::vector<LzssTypedToken> decoded(expected.size());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(direct_plan.event_count),
        direct_plan.decision_count, raw_size, 0};
    const auto decoded_result =
        decode_lzss_contextual_adaptive_huffman_tokens(
            direct_descriptor, direct_payload, parameters, decode_context,
            limits, decode_workspace.nodes, decode_workspace.symbols,
            decoded, variant);
    ASSERT_EQ(decoded_result.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(decoded_result.raw_size, raw_size);
    EXPECT_TRUE(std::ranges::equal(
        expected, decoded, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    ContextualAdaptiveHuffmanDescriptor sentinel{99, 99, 99, 99, 99};
    EXPECT_EQ(plan_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  direct_workspace.nodes, direct_workspace.symbols,
                  sentinel).error,
              LzssContextualAdaptiveHuffmanEncodeError::invalid_parameters);
    EXPECT_EQ(plan_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  direct_workspace.nodes, direct_workspace.symbols,
                  sentinel, static_cast<LzssFieldContextVariant>(99)).error,
              LzssContextualAdaptiveHuffmanEncodeError::invalid_parameters);
    EXPECT_EQ(sentinel.decision_count, 99U);

    EXPECT_EQ(plan_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  std::span{direct_workspace.nodes}.first(
                      direct_workspace.nodes.size() - 1),
                  direct_workspace.symbols, sentinel, variant).error,
              LzssContextualAdaptiveHuffmanEncodeError::
                  node_workspace_too_small);
    EXPECT_EQ(plan_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  direct_workspace.nodes,
                  std::span{direct_workspace.symbols}.first(
                      direct_workspace.symbols.size() - 1),
                  sentinel, variant).error,
              LzssContextualAdaptiveHuffmanEncodeError::
                  symbol_workspace_too_small);

    std::ranges::fill(decoded, LzssTypedToken{
        LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU});
    const auto before = decoded;
    EXPECT_EQ(decode_lzss_contextual_adaptive_huffman_tokens(
                  direct_descriptor, direct_payload, parameters,
                  decode_context, limits,
                  std::span{decode_workspace.nodes}.first(
                      decode_workspace.nodes.size() - 1),
                  decode_workspace.symbols, decoded, variant).error,
              LzssContextualAdaptiveHuffmanDecodeError::
                  node_workspace_too_small);
    EXPECT_EQ(decode_lzss_contextual_adaptive_huffman_tokens(
                  direct_descriptor, direct_payload, parameters,
                  decode_context, limits, decode_workspace.nodes,
                  std::span{decode_workspace.symbols}.first(
                      decode_workspace.symbols.size() - 1),
                  decoded, variant).error,
              LzssContextualAdaptiveHuffmanDecodeError::
                  symbol_workspace_too_small);
    EXPECT_EQ(decode_lzss_contextual_adaptive_huffman_tokens(
                  direct_descriptor, direct_payload, parameters,
                  decode_context, limits, decode_workspace.nodes,
                  decode_workspace.symbols, decoded,
                  static_cast<LzssFieldContextVariant>(99)).error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_parameters);
    const auto crossed = decode_lzss_contextual_adaptive_huffman_tokens(
        direct_descriptor, direct_payload, parameters, decode_context, limits,
        decode_workspace.nodes, decode_workspace.symbols, decoded);
    EXPECT_EQ(crossed.error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_parameters);
    EXPECT_TRUE(std::ranges::equal(
        decoded, before, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     FourMiBLayoutMatchesOperationsAndRejectsOlderLayoutsAtomically) {
    const auto expected = four_mib_distance_tokens();
    constexpr std::uint32_t prefix_size = 4'194'304;
    constexpr std::uint32_t raw_size = prefix_size + 5;
    constexpr auto variant = LzssFieldContextVariant::field_context_4m;
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = prefix_size;
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = raw_size;
    limits.max_block_size = raw_size;
    limits.max_lz_distance = prefix_size;
    limits.max_entropy_table_entries = 13'729;
    const LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(expected.size()), raw_size, 0};

    const auto operation_plan = plan_lzss_field_context_operations(
        expected, parameters, context, limits, variant);
    ASSERT_EQ(operation_plan.error, LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(operation_plan.operation_count);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  expected, parameters, context, limits, operations,
                  variant).error,
              LzssFieldContextError::none);
    ASSERT_GE(operations.size(), 2U);
    EXPECT_EQ(operations[operations.size() - 2].alphabet_size, 23U);
    EXPECT_EQ(operations[operations.size() - 2].value, 22U);
    EXPECT_EQ(operations.back().kind, ModeledOperationKind::bypass_bits);
    EXPECT_EQ(operations.back().bit_count, 22U);
    EXPECT_EQ(operations.back().value, 0U);

    FourMiBWorkspace reference_workspace{};
    ContextualAdaptiveHuffmanDescriptor reference_descriptor{};
    const auto reference_plan = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, limits, reference_workspace.nodes,
            reference_workspace.symbols, reference_descriptor, variant);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_adaptive_huffman_operations(
                      operations, limits, reference_workspace.nodes,
                      reference_workspace.symbols, reference_payload,
                      reference_descriptor, variant).error,
              marc::entropy::internal::
                  ContextualAdaptiveHuffmanEncodeError::none);

    FourMiBWorkspace direct_workspace{};
    ContextualAdaptiveHuffmanDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_adaptive_huffman_tokens(
        expected, parameters, context, limits, direct_workspace.nodes,
        direct_workspace.symbols, direct_descriptor, variant);
    ASSERT_EQ(direct_plan.error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_bits, reference_plan.payload_bits);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());
    expect_descriptor_eq(direct_descriptor, reference_descriptor);

    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, context, limits,
                  direct_workspace.nodes, direct_workspace.symbols,
                  direct_payload, direct_descriptor, variant).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(direct_payload, reference_payload);

    FourMiBWorkspace decode_workspace{};
    std::vector<LzssTypedToken> decoded(expected.size());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(direct_plan.event_count),
        direct_plan.decision_count, raw_size, 0};
    const auto decoded_result =
        decode_lzss_contextual_adaptive_huffman_tokens(
            direct_descriptor, direct_payload, parameters, decode_context,
            limits, decode_workspace.nodes, decode_workspace.symbols,
            decoded, variant);
    ASSERT_EQ(decoded_result.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        expected, decoded, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    std::ranges::fill(decoded, LzssTypedToken{
        LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU, 0xCCCCCCCCU});
    const auto before = decoded;
    for (const auto older : {
             LzssFieldContextVariant::field_context_64k,
             LzssFieldContextVariant::field_context_1m}) {
        const auto crossed = decode_lzss_contextual_adaptive_huffman_tokens(
            direct_descriptor, direct_payload, parameters, decode_context,
            limits, decode_workspace.nodes, decode_workspace.symbols,
            decoded, older);
        EXPECT_EQ(crossed.error,
                  LzssContextualAdaptiveHuffmanDecodeError::
                      invalid_parameters);
        EXPECT_TRUE(std::ranges::equal(
            decoded, before, [](const auto& left, const auto& right) {
                return left.kind == right.kind
                    && left.literal == right.literal
                    && left.distance == right.distance
                    && left.length == right.length;
            }));
    }
}

TEST(LzssContextualAdaptiveHuffmanEncoder,
     SixteenMiBDistanceRoundTripsDirectTypedTokensAtomically) {
    const auto expected = sixteen_mib_distance_tokens();
    constexpr std::uint32_t raw_size = UINT32_C(16777221);
    constexpr auto variant = LzssFieldContextVariant::field_context_16m;
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = UINT32_C(16777216);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = raw_size;
    limits.max_block_size = raw_size;
    limits.max_lz_distance = parameters.window_size;
    limits.max_entropy_table_entries =
        contextual_adaptive_huffman_node_entries_v4
        + contextual_adaptive_huffman_symbol_entries_v4;
    const LzssTypedFrameValidationContext frame_context{
        static_cast<std::uint32_t>(expected.size()), raw_size, 0};

    SixteenMiBWorkspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    const auto plan = plan_lzss_contextual_adaptive_huffman_tokens(
        expected, parameters, frame_context, limits, workspace.nodes,
        workspace.symbols, descriptor, variant);
    ASSERT_EQ(plan.error, LzssContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(plan.token_count, expected.size());
    EXPECT_EQ(plan.event_count, 260'118U);
    EXPECT_EQ(plan.decision_count, 650'309U);

    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_tokens(
                  expected, parameters, frame_context, limits,
                  workspace.nodes, workspace.symbols, payload, descriptor,
                  variant).error,
              LzssContextualAdaptiveHuffmanEncodeError::none);

    std::vector<LzssTypedToken> decoded(expected.size());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(plan.event_count), plan.decision_count,
        raw_size, 0};
    const auto decoded_result =
        decode_lzss_contextual_adaptive_huffman_tokens(
            descriptor, payload, parameters, decode_context, limits,
            workspace.nodes, workspace.symbols, decoded, variant);
    ASSERT_EQ(decoded_result.error,
              LzssContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        expected, decoded, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    std::ranges::fill(decoded, LzssTypedToken{
        LzssTypedTokenKind::match, 0xcc, UINT32_C(0xcccccccc),
        UINT32_C(0xcccccccc)});
    const auto before = decoded;
    const auto crossed = decode_lzss_contextual_adaptive_huffman_tokens(
        descriptor, payload, parameters, decode_context, limits,
        workspace.nodes, workspace.symbols, decoded,
        LzssFieldContextVariant::field_context_4m);
    EXPECT_EQ(crossed.error,
              LzssContextualAdaptiveHuffmanDecodeError::invalid_parameters);
    EXPECT_TRUE(std::ranges::equal(
        decoded, before, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
}

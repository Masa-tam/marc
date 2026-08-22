#include "entropy/contextual_adaptive_huffman_encoder.hpp"

#include "entropy/contextual_adaptive_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using namespace marc::entropy::internal;

inline constexpr auto legacy_context_variant =
    LzssFieldContextVariant::field_context_64k;

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult
plan_contextual_adaptive_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const marc::core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    return marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, limits, nodes, symbols, descriptor,
            legacy_context_variant);
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult
encode_contextual_adaptive_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const marc::core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<std::byte> payload,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    return marc::entropy::internal::
        encode_contextual_adaptive_huffman_operations(
            operations, limits, nodes, symbols, payload, descriptor,
            legacy_context_variant);
}

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

struct SelectedWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries_v2>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries_v2>
        symbols{};
};

struct FourMiBWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries_v3>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries_v3>
        symbols{};
};

[[nodiscard]] constexpr ModeledOperation symbol(
    const std::uint16_t context, const std::uint16_t alphabet,
    const std::uint32_t value) {
    return {ModeledOperationKind::symbol, context, alphabet, value, 0};
}

[[nodiscard]] constexpr ModeledOperation bypass(
    const std::uint8_t bits, const std::uint32_t value) {
    return {ModeledOperationKind::bypass_bits, 0, 0, value, bits};
}

[[nodiscard]] constexpr auto literal_operations() {
    return std::array{symbol(0, 2, 0), symbol(3, 256, 'A')};
}

[[nodiscard]] constexpr auto mixed_operations() {
    return std::array{
        symbol(0, 2, 0), symbol(0, 2, 0), symbol(0, 2, 1),
        bypass(3, 5), symbol(3, 256, 'A'), symbol(3, 256, 'A')};
}

[[nodiscard]] constexpr auto selected_operations() {
    return std::array{symbol(23, 21, 20), bypass(20, UINT32_C(0xabcde))};
}

[[nodiscard]] constexpr auto four_mib_operations() {
    return std::array{symbol(23, 23, 22), bypass(22, 0)};
}

} // namespace

TEST(ContextualAdaptiveHuffmanEncoder, PlansAndEncodesDocumentedLiteral) {
    constexpr auto operations = literal_operations();
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto result = plan_contextual_adaptive_huffman_operations(
        operations, {}, workspace.nodes, workspace.symbols, descriptor);
    ASSERT_EQ(result.error, ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_bits, 9U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(descriptor.decision_count, 2U);
    EXPECT_EQ(descriptor.payload_size, 2U);
    EXPECT_EQ(descriptor.final_valid_bits, 1U);

    std::array payload{
        std::byte{0xCC}, std::byte{0xCC}, std::byte{0xCC}};
    descriptor = {99, 99, 99, 99, 99};
    result = encode_contextual_adaptive_huffman_operations(
        operations, {}, workspace.nodes, workspace.symbols, payload,
        descriptor);
    ASSERT_EQ(result.error, ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(payload[0], std::byte{0x82});
    EXPECT_EQ(payload[1], std::byte{0x00});
    EXPECT_EQ(payload[2], std::byte{0xCC});
    EXPECT_EQ(descriptor.final_valid_bits, 1U);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     ExistingSymbolsAndBypassDecodeInLockstep) {
    constexpr auto operations = mixed_operations();
    Workspace encode_workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    const auto plan = plan_contextual_adaptive_huffman_operations(
        operations, {}, encode_workspace.nodes, encode_workspace.symbols,
        descriptor);
    ASSERT_EQ(plan.error, ContextualAdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_contextual_adaptive_huffman_operations(
                  operations, {}, encode_workspace.nodes,
                  encode_workspace.symbols, payload, descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::none);

    Workspace decode_workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, payload, {}, decode_workspace.nodes,
                  decode_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{0xCCCCCCCCU};
        const auto decoded = operation.kind == ModeledOperationKind::symbol
            ? decoder.decode_symbol(
                  operation.context_id, operation.alphabet_size, value)
            : decoder.decode_bypass(operation.bit_count, value);
        ASSERT_EQ(decoded.error, ContextualAdaptiveHuffmanDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    EXPECT_EQ(decoder.finish(
                  static_cast<std::uint32_t>(operations.size()),
                  descriptor.decision_count).error,
              ContextualAdaptiveHuffmanDecodeError::none);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     RejectsMalformedOperationsAtStableIndexTransactionally) {
    struct Case {
        ModeledOperation operation;
        ContextualAdaptiveHuffmanEncodeError error;
    };
    constexpr std::array cases{
        Case{{static_cast<ModeledOperationKind>(2), 0, 2, 0, 0},
             ContextualAdaptiveHuffmanEncodeError::invalid_operation_kind},
        Case{{ModeledOperationKind::symbol, 31, 2, 0, 0},
             ContextualAdaptiveHuffmanEncodeError::invalid_context},
        Case{{ModeledOperationKind::symbol, 0, 3, 0, 0},
             ContextualAdaptiveHuffmanEncodeError::invalid_alphabet},
        Case{{ModeledOperationKind::symbol, 0, 2, 2, 0},
             ContextualAdaptiveHuffmanEncodeError::invalid_symbol},
        Case{{ModeledOperationKind::symbol, 0, 2, 0, 1},
             ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field},
        Case{{ModeledOperationKind::bypass_bits, 1, 0, 0, 1},
             ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field},
        Case{{ModeledOperationKind::bypass_bits, 0, 0, 0, 0},
             ContextualAdaptiveHuffmanEncodeError::invalid_bypass_width},
        Case{{ModeledOperationKind::bypass_bits, 0, 0, 4, 2},
             ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field},
    };
    for (const auto& test : cases) {
        Workspace workspace{};
        constexpr auto prefix = literal_operations();
        std::array operations{prefix[0], test.operation};
        ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
        const auto result = plan_contextual_adaptive_huffman_operations(
            operations, {}, workspace.nodes, workspace.symbols, descriptor);
        EXPECT_EQ(result.error, test.error);
        EXPECT_EQ(result.operation_count, 1U);
        EXPECT_EQ(result.operation_index, 1U);
        EXPECT_EQ(descriptor.decision_count, 99U);
    }
}

TEST(ContextualAdaptiveHuffmanEncoder,
     CapacityFailuresPreservePayloadAndDescriptor) {
    constexpr auto operations = literal_operations();
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto result = plan_contextual_adaptive_huffman_operations(
        operations, {},
        std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
        workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::node_workspace_too_small);
    result = plan_contextual_adaptive_huffman_operations(
        operations, {}, workspace.nodes,
        std::span{workspace.symbols}.first(workspace.symbols.size() - 1),
        descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::
                  symbol_workspace_too_small);

    std::array<std::byte, 2> payload{
        std::byte{0xCC}, std::byte{0xCC}};
    result = encode_contextual_adaptive_huffman_operations(
        operations, {}, workspace.nodes, workspace.symbols,
        std::span{payload}.first(1), descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::
                  payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xCC};
    }));
    EXPECT_EQ(descriptor.decision_count, 99U);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     RejectsOperationModelAndOutputAliasesBeforePublication) {
    Workspace workspace{};
    auto node_bytes = std::as_writable_bytes(std::span{workspace.nodes});
    auto* stored_operation =
        reinterpret_cast<ModeledOperation*>(node_bytes.data());
    *stored_operation = symbol(0, 2, 0);
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    auto result = plan_contextual_adaptive_huffman_operations(
        std::span<const ModeledOperation>{stored_operation, 1}, {},
        workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::overlapping_buffers);

    constexpr auto operations = literal_operations();
    Workspace output_workspace{};
    auto output_bytes = std::as_writable_bytes(
        std::span{output_workspace.nodes});
    result = encode_contextual_adaptive_huffman_operations(
        operations, {}, output_workspace.nodes, output_workspace.symbols,
        output_bytes.first(2), descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::overlapping_buffers);

    std::array operation_output_storage{operations[0], operations[1]};
    auto operation_output_bytes =
        std::as_writable_bytes(std::span{operation_output_storage});
    Workspace operation_output_workspace{};
    result = encode_contextual_adaptive_huffman_operations(
        operation_output_storage, {}, operation_output_workspace.nodes,
        operation_output_workspace.symbols,
        operation_output_bytes.first(2), descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::overlapping_buffers);

    Workspace model_workspace{};
    auto overlapping_symbols = std::span<std::uint16_t>{
        reinterpret_cast<std::uint16_t*>(model_workspace.nodes.data()),
        contextual_adaptive_huffman_symbol_entries};
    result = plan_contextual_adaptive_huffman_operations(
        operations, {}, model_workspace.nodes, overlapping_symbols,
        descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::overlapping_buffers);
}

TEST(ContextualAdaptiveHuffmanEncoder, EnforcesEmptyAndLocalLimits) {
    Workspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto result = plan_contextual_adaptive_huffman_operations(
        {}, {}, workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::empty_operations);

    constexpr auto operations = literal_operations();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_adaptive_huffman_node_entries
        + contextual_adaptive_huffman_symbol_entries - 1;
    result = plan_contextual_adaptive_huffman_operations(
        operations, limits, workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 1;
    result = plan_contextual_adaptive_huffman_operations(
        operations, limits, workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits = {};
    const auto aggregate = operations.size() * sizeof(ModeledOperation)
        + workspace.nodes.size() * sizeof(AdaptiveHuffmanNode)
        + workspace.symbols.size() * sizeof(std::uint16_t);
    limits.max_block_size = aggregate - 1;
    limits.max_internal_buffered_bytes = aggregate - 1;
    result = plan_contextual_adaptive_huffman_operations(
        operations, limits, workspace.nodes, workspace.symbols, descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::limit_exceeded);

    limits.max_internal_buffered_bytes = aggregate;
    limits.max_block_size = aggregate;
    std::array<std::byte, 2> payload{
        std::byte{0xCC}, std::byte{0xCC}};
    result = encode_contextual_adaptive_huffman_operations(
        operations, limits, workspace.nodes, workspace.symbols, payload,
        descriptor);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xCC};
    }));
    EXPECT_EQ(descriptor.decision_count, 99U);
}

TEST(ContextualAdaptiveHuffmanEncoder, DeterministicAcrossFreshWorkspaces) {
    constexpr auto operations = mixed_operations();
    Workspace first_workspace{};
    Workspace second_workspace{};
    ContextualAdaptiveHuffmanDescriptor first_descriptor{};
    ContextualAdaptiveHuffmanDescriptor second_descriptor{};
    const auto first_plan = plan_contextual_adaptive_huffman_operations(
        operations, {}, first_workspace.nodes, first_workspace.symbols,
        first_descriptor);
    const auto second_plan = plan_contextual_adaptive_huffman_operations(
        operations, {}, second_workspace.nodes, second_workspace.symbols,
        second_descriptor);
    ASSERT_EQ(first_plan.error, ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(second_plan.error, ContextualAdaptiveHuffmanEncodeError::none);
    std::vector<std::byte> first(first_plan.payload_size);
    std::vector<std::byte> second(second_plan.payload_size);
    ASSERT_EQ(encode_contextual_adaptive_huffman_operations(
                  operations, {}, first_workspace.nodes,
                  first_workspace.symbols, first, first_descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(encode_contextual_adaptive_huffman_operations(
                  operations, {}, second_workspace.nodes,
                  second_workspace.symbols, second, second_descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first_descriptor.decision_count,
              second_descriptor.decision_count);
    EXPECT_EQ(first_descriptor.final_valid_bits,
              second_descriptor.final_valid_bits);
}

TEST(ContextualAdaptiveHuffmanEncoder, EnforcesForwardLifecycle) {
    Workspace workspace{};
    ContextualAdaptiveHuffmanForwardEncoder encoder;
    EXPECT_EQ(encoder.encode_symbol(0, 2, 0).error,
              ContextualAdaptiveHuffmanEncodeError::not_started);

    ASSERT_EQ(encoder.begin_plan(
                  {}, workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    EXPECT_EQ(encoder.finish_plan(descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::empty_operations);
    EXPECT_EQ(encoder.encode_symbol(0, 2, 0).error,
              ContextualAdaptiveHuffmanEncodeError::empty_operations);

    ASSERT_EQ(encoder.begin_plan(
                  {}, workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(encoder.encode_symbol(0, 2, 0).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(encoder.finish_plan(descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(encoder.encode_symbol(0, 2, 0).error,
              ContextualAdaptiveHuffmanEncodeError::already_finished);

    descriptor.context_count = 30;
    std::array<std::byte, 1> payload{};
    EXPECT_EQ(encoder.begin_write(
                  descriptor, {}, workspace.nodes, workspace.symbols,
                  payload).error,
              ContextualAdaptiveHuffmanEncodeError::invalid_descriptor);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     SelectedLayoutEncodesAndDecodesClassTwentyHandVector) {
    constexpr auto operations = selected_operations();
    constexpr auto variant = LzssFieldContextVariant::field_context_1m;
    SelectedWorkspace plan_workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {}, plan_workspace.nodes, plan_workspace.symbols,
            descriptor, variant);
    ASSERT_EQ(plan.error, ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(plan.operation_count, 2U);
    EXPECT_EQ(plan.decision_count, 21U);
    EXPECT_EQ(plan.payload_bits, 25U);
    EXPECT_EQ(plan.payload_size, 4U);
    EXPECT_EQ(descriptor.decision_count, 21U);
    EXPECT_EQ(descriptor.payload_size, 4U);
    EXPECT_EQ(descriptor.final_valid_bits, 1U);

    SelectedWorkspace encode_workspace{};
    std::array<std::byte, 4> payload{};
    const auto encoded = marc::entropy::internal::
        encode_contextual_adaptive_huffman_operations(
            operations, {}, encode_workspace.nodes, encode_workspace.symbols,
            payload, descriptor, variant);
    ASSERT_EQ(encoded.error, ContextualAdaptiveHuffmanEncodeError::none);
    constexpr std::array expected{
        std::byte{0xd4}, std::byte{0x9b}, std::byte{0x57}, std::byte{0x01}};
    EXPECT_EQ(payload, expected);

    SelectedWorkspace second_workspace{};
    ContextualAdaptiveHuffmanDescriptor second_descriptor{};
    std::array<std::byte, 4> second_payload{};
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_adaptive_huffman_operations(
                      operations, {}, second_workspace.nodes,
                      second_workspace.symbols, second_payload,
                      second_descriptor, variant).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(second_payload, payload);
    EXPECT_EQ(second_descriptor.decision_count, descriptor.decision_count);
    EXPECT_EQ(second_descriptor.payload_size, descriptor.payload_size);
    EXPECT_EQ(second_descriptor.final_valid_bits,
              descriptor.final_valid_bits);

    SelectedWorkspace decode_workspace{};
    marc::entropy::internal::ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, payload, {}, decode_workspace.nodes,
                  decode_workspace.symbols, variant).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(23, 21, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 20U);
    const auto bypass_result = decoder.decode_bypass(20, value);
    ASSERT_EQ(bypass_result.error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, UINT32_C(0xabcde));
    EXPECT_EQ(bypass_result.event_count, 2U);
    EXPECT_EQ(bypass_result.decision_count, 21U);
    EXPECT_EQ(bypass_result.bits_consumed, 25U);
    EXPECT_EQ(decoder.finish(2, 21).error,
              ContextualAdaptiveHuffmanDecodeError::none);

    SelectedWorkspace forward_plan_workspace{};
    ContextualAdaptiveHuffmanForwardEncoder forward_plan;
    ASSERT_EQ(forward_plan.begin_plan(
                  {}, forward_plan_workspace.nodes,
                  forward_plan_workspace.symbols, variant).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(forward_plan.encode_symbol(23, 21, 20).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(forward_plan.encode_bypass(20, UINT32_C(0xabcde)).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ContextualAdaptiveHuffmanDescriptor forward_descriptor{};
    ASSERT_EQ(forward_plan.finish_plan(forward_descriptor).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(forward_descriptor.decision_count, descriptor.decision_count);
    EXPECT_EQ(forward_descriptor.payload_size, descriptor.payload_size);
    EXPECT_EQ(forward_descriptor.final_valid_bits,
              descriptor.final_valid_bits);

    SelectedWorkspace forward_write_workspace{};
    std::array<std::byte, 4> forward_payload{};
    ContextualAdaptiveHuffmanForwardEncoder forward_write;
    ASSERT_EQ(forward_write.begin_write(
                  forward_descriptor, {}, forward_write_workspace.nodes,
                  forward_write_workspace.symbols, forward_payload,
                  variant).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(forward_write.encode_symbol(23, 21, 20).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(forward_write.encode_bypass(20, UINT32_C(0xabcde)).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    ASSERT_EQ(forward_write.finish_write(2, 21, 25).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(forward_payload, expected);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     SelectedLayoutRejectsLegacyCrossingAndInvalidSelectionAtomically) {
    constexpr auto operations = selected_operations();
    SelectedWorkspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{99, 99, 99, 99, 99};
    auto result = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {}, workspace.nodes, workspace.symbols, descriptor,
            LzssFieldContextVariant::field_context_64k);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::invalid_alphabet);
    EXPECT_EQ(descriptor.decision_count, 99U);

    result = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {}, workspace.nodes, workspace.symbols, descriptor,
            static_cast<LzssFieldContextVariant>(UINT16_C(0xffff)));
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::invalid_context_variant);
    EXPECT_EQ(descriptor.decision_count, 99U);

    constexpr auto variant = LzssFieldContextVariant::field_context_1m;
    result = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {},
            std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
            workspace.symbols, descriptor, variant);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::node_workspace_too_small);
    result = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {}, workspace.nodes,
            std::span{workspace.symbols}.first(workspace.symbols.size() - 1),
            descriptor, variant);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::symbol_workspace_too_small);
    EXPECT_EQ(descriptor.decision_count, 99U);

    std::array<std::byte, 4> payload{
        std::byte{0xcc}, std::byte{0xcc}, std::byte{0xcc}, std::byte{0xcc}};
    result = marc::entropy::internal::
        encode_contextual_adaptive_huffman_operations(
            operations, {},
            std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
            workspace.symbols, payload, descriptor, variant);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanEncodeError::node_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto byte) {
        return byte == std::byte{0xcc};
    }));

    constexpr std::array encoded{
        std::byte{0xd4}, std::byte{0x9b}, std::byte{0x57}, std::byte{0x01}};
    const ContextualAdaptiveHuffmanDescriptor selected_descriptor{
        21, 4, 31, 1, 0};
    marc::entropy::internal::ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  selected_descriptor, encoded, {}, workspace.nodes,
                  workspace.symbols,
                  LzssFieldContextVariant::field_context_64k).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    std::uint32_t value{UINT32_C(0xcccccccc)};
    EXPECT_EQ(decoder.decode_symbol(23, 21, value).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_alphabet);
    EXPECT_EQ(value, UINT32_C(0xcccccccc));

    marc::entropy::internal::ContextualAdaptiveHuffmanDecoder invalid_decoder;
    EXPECT_EQ(invalid_decoder.begin(
                  selected_descriptor, encoded, {}, workspace.nodes,
                  workspace.symbols,
                  static_cast<LzssFieldContextVariant>(UINT16_C(0xffff))).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_context_variant);
}

TEST(ContextualAdaptiveHuffmanEncoder,
     FourMiBLayoutEncodesAndDecodesClassTwentyTwoHandVector) {
    constexpr auto operations = four_mib_operations();
    constexpr auto variant = LzssFieldContextVariant::field_context_4m;
    FourMiBWorkspace workspace{};
    ContextualAdaptiveHuffmanDescriptor descriptor{};
    const auto plan = marc::entropy::internal::
        plan_contextual_adaptive_huffman_operations(
            operations, {}, workspace.nodes, workspace.symbols, descriptor,
            variant);
    ASSERT_EQ(plan.error, ContextualAdaptiveHuffmanEncodeError::none);
    EXPECT_EQ(plan.operation_count, 2U);
    EXPECT_EQ(plan.decision_count, 23U);
    EXPECT_EQ(plan.payload_bits, 27U);
    EXPECT_EQ(plan.payload_size, 4U);
    EXPECT_EQ(descriptor.final_valid_bits, 3U);

    std::array<std::byte, 4> payload{};
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_adaptive_huffman_operations(
                      operations, {}, workspace.nodes, workspace.symbols,
                      payload, descriptor, variant).error,
              ContextualAdaptiveHuffmanEncodeError::none);
    constexpr std::array expected{
        std::byte{0x16}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(payload, expected);

    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, payload, {}, workspace.nodes,
                  workspace.symbols, variant).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(23, 23, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 22U);
    ASSERT_EQ(decoder.decode_bypass(22, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(decoder.finish(2, 23).error,
              ContextualAdaptiveHuffmanDecodeError::none);

    ContextualAdaptiveHuffmanDecoder crossed;
    ASSERT_EQ(crossed.begin(
                  descriptor, payload, {}, workspace.nodes,
                  workspace.symbols,
                  LzssFieldContextVariant::field_context_1m).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    value = UINT32_C(0xcccccccc);
    EXPECT_EQ(crossed.decode_symbol(23, 23, value).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_alphabet);
    EXPECT_EQ(value, UINT32_C(0xcccccccc));
}

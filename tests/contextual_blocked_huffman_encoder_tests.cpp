#include "entropy/contextual_blocked_huffman_encoder.hpp"

#include "entropy/contextual_blocked_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>

namespace {

using namespace marc::entropy::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;

constexpr ModeledOperation symbol(
    const std::uint16_t context, const std::uint16_t alphabet,
    const std::uint32_t value) {
    return {ModeledOperationKind::symbol, context, alphabet, value, 0};
}

constexpr ModeledOperation bypass(
    const std::uint8_t bits, const std::uint32_t value) {
    return {ModeledOperationKind::bypass_bits, 0, 0, value, bits};
}

} // namespace

TEST(ContextualBlockedHuffmanEncoder, PlansDocumentedOneLiteralVector) {
    constexpr std::array operations{symbol(0, 2, 0), symbol(3, 256, 'A')};
    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto result = plan_contextual_blocked_huffman_operations(
        operations, {}, descriptor);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.descriptor_size, 24U);
    EXPECT_EQ(result.payload_size, 0U);
    EXPECT_EQ(descriptor.field_active_mask, 0x03U);
    EXPECT_EQ(descriptor.override_mask, 0U);
    EXPECT_EQ(descriptor.field_models[0].single_symbol, 0U);
    EXPECT_EQ(descriptor.field_models[1].single_symbol, 'A');
}

TEST(ContextualBlockedHuffmanEncoder, EmitsMixedCodesAndBypassLsbFirst) {
    constexpr std::array operations{
        symbol(0, 2, 0), symbol(3, 256, 'A'), symbol(1, 2, 1),
        symbol(21, 8, 1), bypass(1, 1), symbol(24, 17, 1), bypass(1, 0)};
    std::array output{std::byte{0xcc}, std::byte{0xcc}};
    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto result = encode_contextual_blocked_huffman_operations(
        operations, {}, output, descriptor);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.decision_count, 7U);
    EXPECT_EQ(result.payload_size, 1U);
    EXPECT_EQ(descriptor.final_valid_bits, 4U);
    EXPECT_EQ(output[0], std::byte{0x06});
    EXPECT_EQ(output[1], std::byte{0xcc});

    std::array<HuffmanDecodeTable, 1> tables{};
    ContextualBlockedHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, std::span<const std::byte>{output}.first(1), {},
                  tables).error,
              ContextualBlockedHuffmanDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{};
        const auto decoded = operation.kind == ModeledOperationKind::symbol
            ? decoder.decode_symbol(
                  operation.context_id, operation.alphabet_size, value)
            : decoder.decode_bypass(operation.bit_count, value);
        ASSERT_EQ(decoded.error, ContextualBlockedHuffmanDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    EXPECT_EQ(decoder.finish(operations.size(), 7).error,
              ContextualBlockedHuffmanDecodeError::none);
}

TEST(ContextualBlockedHuffmanEncoder, SelectsOnlyStrictlyProfitableOverrides) {
    std::array<ModeledOperation, 81> operations{};
    for (std::size_t index = 0; index < 40; ++index) {
        operations[index] = symbol(0, 2, 0);
        operations[40 + index] = symbol(1, 2, 1);
    }
    operations[80] = symbol(3, 256, 'A');
    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto result = plan_contextual_blocked_huffman_operations(
        operations, {}, descriptor);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(descriptor.field_active_mask, 0x03U);
    EXPECT_EQ(descriptor.override_mask, 0x03U);
    EXPECT_EQ(descriptor.context_models[0].single_symbol, 0U);
    EXPECT_EQ(descriptor.context_models[1].single_symbol, 1U);
    EXPECT_EQ(result.descriptor_size, 33U);
    EXPECT_EQ(result.payload_size, 0U);
}

TEST(ContextualBlockedHuffmanEncoder, FailuresAreAtomic) {
    constexpr std::array mixed{
        symbol(0, 2, 0), symbol(3, 256, 'A'), symbol(1, 2, 1)};
    std::array output{std::byte{0xcc}, std::byte{0xcc}};
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 0xCCCCCCCCU;
    auto result = encode_contextual_blocked_huffman_operations(
        mixed, {}, std::span<std::byte>{}, descriptor);
    EXPECT_EQ(result.error,
              ContextualBlockedHuffmanEncodeError::payload_output_too_small);
    EXPECT_EQ(descriptor.decision_count, 0xCCCCCCCCU);
    EXPECT_EQ(output[0], std::byte{0xcc});

    constexpr std::array invalid{
        ModeledOperation{ModeledOperationKind::symbol, 31, 2, 0, 0}};
    result = plan_contextual_blocked_huffman_operations(
        invalid, {}, descriptor);
    EXPECT_EQ(result.error,
              ContextualBlockedHuffmanEncodeError::invalid_operation);
    EXPECT_EQ(result.operation_index, 0U);

    constexpr std::array incomplete{symbol(0, 2, 0)};
    result = plan_contextual_blocked_huffman_operations(
        incomplete, {}, descriptor);
    EXPECT_EQ(result.error,
              ContextualBlockedHuffmanEncodeError::invalid_operation);

    std::array operation_storage{symbol(0, 2, 0)};
    auto operation_bytes =
        std::as_writable_bytes(std::span{operation_storage});
    result = encode_contextual_blocked_huffman_operations(
        operation_storage, {}, operation_bytes, descriptor);
    EXPECT_EQ(result.error,
              ContextualBlockedHuffmanEncodeError::overlapping_buffers);
}

#include "entropy/contextual_huffman_estimator.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {

using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::entropy::internal::ContextualHuffmanEstimateError;
using marc::entropy::internal::estimate_contextual_huffman_cost;

[[nodiscard]] constexpr ModeledOperation symbol(
    const std::uint16_t context, const std::uint16_t alphabet,
    const std::uint32_t value) {
    return {ModeledOperationKind::symbol, context, alphabet, value, 0};
}

[[nodiscard]] constexpr ModeledOperation bypass(
    const std::uint8_t bits, const std::uint32_t value) {
    return {ModeledOperationKind::bypass_bits, 0, 0, value, bits};
}

} // namespace

TEST(ContextualHuffmanEstimator, EmptyInputChargesOnlyCandidateMetadata) {
    const auto result = estimate_contextual_huffman_cost({});
    ASSERT_EQ(result.error, ContextualHuffmanEstimateError::none);
    EXPECT_EQ(result.estimates.field_tables.total_bytes, 8U);
    EXPECT_EQ(result.estimates.contextual_tables.total_bytes, 8U);
    EXPECT_EQ(result.estimates.shared_contextual_tables.total_bytes, 39U);
}

TEST(ContextualHuffmanEstimator, CountsDescriptorPayloadAndBypassExactly) {
    constexpr std::array operations{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 0), symbol(8, 256, 'B'),
        symbol(1, 2, 1), symbol(21, 8, 2), bypass(2, 2),
        symbol(25, 17, 1), bypass(1, 0),
        symbol(2, 2, 0), symbol(8, 256, 'C')};
    const auto result = estimate_contextual_huffman_cost(operations);
    ASSERT_EQ(result.error, ContextualHuffmanEstimateError::none);

    const auto& field = result.estimates.field_tables;
    EXPECT_EQ(field.active_tables, 4U);
    EXPECT_EQ(field.stored_models, 4U);
    EXPECT_EQ(field.descriptor_bytes, 31U);
    EXPECT_EQ(field.symbol_bits, 9U);
    EXPECT_EQ(field.bypass_bits, 3U);
    EXPECT_EQ(field.payload_bytes, 2U);
    EXPECT_EQ(field.total_bytes, 33U);

    const auto& contextual = result.estimates.contextual_tables;
    EXPECT_EQ(contextual.active_tables, 7U);
    EXPECT_EQ(contextual.stored_models, 7U);
    EXPECT_EQ(contextual.descriptor_bytes, 41U);
    EXPECT_EQ(contextual.symbol_bits, 4U);
    EXPECT_EQ(contextual.bypass_bits, 3U);
    EXPECT_EQ(contextual.payload_bytes, 1U);
    EXPECT_EQ(contextual.total_bytes, 42U);

    const auto& shared = result.estimates.shared_contextual_tables;
    EXPECT_EQ(shared.active_tables, 7U);
    EXPECT_EQ(shared.stored_models, 6U);
    EXPECT_EQ(shared.descriptor_bytes, 68U);
    EXPECT_EQ(shared.symbol_bits, 4U);
    EXPECT_EQ(shared.payload_bytes, 1U);
    EXPECT_EQ(shared.total_bytes, 69U);
}

TEST(ContextualHuffmanEstimator, RejectsMalformedOperationsAtExactIndex) {
    constexpr std::array operations{
        symbol(0, 2, 0),
        ModeledOperation{ModeledOperationKind::symbol, 3, 255, 'A', 0}};
    const auto result = estimate_contextual_huffman_cost(operations);
    EXPECT_EQ(result.error,
              ContextualHuffmanEstimateError::invalid_operation);
    EXPECT_EQ(result.operation_index, 1U);
}

TEST(ContextualHuffmanEstimator, SingleSymbolModelsConsumeNoPayloadBits) {
    constexpr std::array operations{
        symbol(0, 2, 0), symbol(0, 2, 0), symbol(0, 2, 0)};
    const auto result = estimate_contextual_huffman_cost(operations);
    ASSERT_EQ(result.error, ContextualHuffmanEstimateError::none);
    EXPECT_EQ(result.estimates.field_tables.symbol_bits, 0U);
    EXPECT_EQ(result.estimates.contextual_tables.symbol_bits, 0U);
    EXPECT_EQ(result.estimates.field_tables.total_bytes, 12U);
    EXPECT_EQ(result.estimates.contextual_tables.total_bytes, 12U);
}

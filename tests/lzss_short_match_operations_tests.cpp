#include "context/lzss_short_match_operations.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

constexpr marc::dictionary::internal::LzssParameters parameters{
    65536, 3, 258, 0};

void expect_symbol(const ModeledOperation& operation,
                   const std::uint16_t context, const std::uint16_t alphabet,
                   const std::uint32_t value) {
    EXPECT_EQ(operation.kind, ModeledOperationKind::symbol);
    EXPECT_EQ(operation.context_id, context);
    EXPECT_EQ(operation.alphabet_size, alphabet);
    EXPECT_EQ(operation.value, value);
    EXPECT_EQ(operation.bit_count, 0U);
}

void expect_bypass(const ModeledOperation& operation,
                   const std::uint8_t bits, const std::uint32_t value) {
    EXPECT_EQ(operation.kind, ModeledOperationKind::bypass_bits);
    EXPECT_EQ(operation.context_id, 0U);
    EXPECT_EQ(operation.alphabet_size, 0U);
    EXPECT_EQ(operation.bit_count, bits);
    EXPECT_EQ(operation.value, value);
}

} // namespace

TEST(LzssShortMatchOperations, ModelsHandVector) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};
    const auto limits = marc::core::DecoderLimits{};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        2, 4, 0};
    const auto plan = plan_lzss_short_match_operations(
        tokens, parameters, context, limits);
    ASSERT_EQ(plan.error, LzssFieldContextError::none);
    EXPECT_EQ(plan.operation_count, 5U);
    EXPECT_EQ(plan.decision_count, 5U);

    std::array<ModeledOperation, 5> operations{};
    const auto modeled = model_lzss_short_match_tokens(
        tokens, parameters, context, limits, operations);
    ASSERT_EQ(modeled.error, LzssFieldContextError::none);
    EXPECT_EQ(modeled.operation_count, plan.operation_count);
    EXPECT_EQ(modeled.decision_count, plan.decision_count);
    expect_symbol(operations[0], 0, 2, 0);
    expect_symbol(operations[1], 3, 256, 0x61);
    expect_symbol(operations[2], 1, 2, 1);
    expect_symbol(operations[3], 21, 9, 0);
    expect_symbol(operations[4], 23, 17, 0);
}

TEST(LzssShortMatchOperations, ModelsFourAndMaximumLength) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 4},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 4, 258}};
    const auto limits = marc::core::DecoderLimits{};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        3, 263, 0};
    std::array<ModeledOperation, 11> operations{};
    const auto result = model_lzss_short_match_tokens(
        tokens, parameters, context, limits, operations);
    ASSERT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 11U);
    EXPECT_EQ(result.decision_count, 19U);
    expect_symbol(operations[3], 21, 9, 1);
    expect_bypass(operations[4], 1, 0);
    expect_symbol(operations[5], 24, 17, 0);
    expect_symbol(operations[6], 2, 2, 1);
    expect_symbol(operations[7], 22, 9, 8);
    expect_bypass(operations[8], 8, 0);
    expect_symbol(operations[9], 31, 17, 2);
    expect_bypass(operations[10], 2, 0);
}

TEST(LzssShortMatchOperations, RejectsInvalidAndInsufficientOutputBeforeWrite) {
    auto tokens = std::array{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};
    const auto limits = marc::core::DecoderLimits{};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        2, 4, 0};
    std::array<ModeledOperation, 5> operations{};
    for (auto& operation : operations) operation.value = 0xdead;
    tokens[1].distance = 2;
    auto result = model_lzss_short_match_tokens(
        tokens, parameters, context, limits, operations);
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    for (const auto& operation : operations) {
        EXPECT_EQ(operation.value, 0xdeadU);
    }
    tokens[1].distance = 1;
    result = model_lzss_short_match_tokens(
        tokens, parameters, context, limits,
        std::span<ModeledOperation>{operations}.first(4));
    EXPECT_EQ(result.error, LzssFieldContextError::output_too_small);
    for (const auto& operation : operations) {
        EXPECT_EQ(operation.value, 0xdeadU);
    }
}

TEST(LzssShortMatchOperations, RejectsAliasedTokenAndOperationStorage) {
    constexpr std::array input{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};
    alignas(ModeledOperation) std::array<std::byte,
        5 * sizeof(ModeledOperation)> memory{};
    auto* tokens = reinterpret_cast<LzssTypedToken*>(memory.data());
    tokens[0] = input[0];
    tokens[1] = input[1];
    auto* operations = reinterpret_cast<ModeledOperation*>(memory.data());
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        2, 4, 0};
    const auto result = model_lzss_short_match_tokens(
        std::span<const LzssTypedToken>{tokens, 2}, parameters, context,
        marc::core::DecoderLimits{},
        std::span<ModeledOperation>{operations, 5});
    EXPECT_EQ(result.error, LzssFieldContextError::overlapping_buffers);
}

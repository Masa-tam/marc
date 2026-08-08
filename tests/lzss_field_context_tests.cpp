#include "context/lzss_field_context.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenError;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] constexpr ModeledOperation symbol(
    const std::uint16_t context, const std::uint16_t alphabet,
    const std::uint32_t value) {
    return {ModeledOperationKind::symbol, context, alphabet, value, 0};
}

[[nodiscard]] constexpr ModeledOperation bypass(
    const std::uint8_t bits, const std::uint32_t value) {
    return {ModeledOperationKind::bypass_bits, 0, 0, value, bits};
}

[[nodiscard]] constexpr auto one_literal() {
    return std::array{symbol(0, 2, 0), symbol(3, 256, 'A')};
}

[[nodiscard]] constexpr auto stateful_sequence() {
    return std::array{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 0), symbol(8, 256, 'B'),
        symbol(1, 2, 1), symbol(21, 8, 2), bypass(2, 2),
        symbol(25, 17, 1), bypass(1, 0),
        symbol(2, 2, 0), symbol(8, 256, 'C')};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

} // namespace

TEST(LzssFieldContext, AcceptsEmptyFrameWithoutWritingOutput) {
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 0xCC, 0, 0}};
    const auto before = tokens;
    const auto result = invert_lzss_field_context_operations(
        {}, {}, {}, marc::core::DecoderLimits{}, tokens);
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 0U);
    EXPECT_EQ(result.decision_count, 0U);
    EXPECT_EQ(result.token_count, 0U);
    EXPECT_EQ(result.raw_size, 0U);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
}

TEST(LzssFieldContext, InvertsSpecifiedOneLiteralVector) {
    constexpr auto operations = one_literal();
    std::array<LzssTypedToken, 1> tokens{};
    const auto result = invert_lzss_field_context_operations(
        operations, {}, literal_context(), marc::core::DecoderLimits{},
        tokens);
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
}

TEST(LzssFieldContext, TracksPreviousKindLiteralNibbleAndBypassBits) {
    constexpr auto operations = stateful_sequence();
    std::array<LzssTypedToken, 4> tokens{};
    const LzssFieldContextValidationContext context{4, 11, 12, 13, 0};
    const auto result = invert_lzss_field_context_operations(
        operations, {}, context, marc::core::DecoderLimits{}, tokens);
    ASSERT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 'B');
    EXPECT_EQ(tokens[2].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[2].length, 10U);
    EXPECT_EQ(tokens[2].distance, 2U);
    EXPECT_EQ(tokens[3].literal, 'C');
    EXPECT_EQ(result.decision_count, 12U);
    EXPECT_EQ(result.raw_size, 13U);
}

TEST(LzssFieldContext, ValidationDoesNotRequireTokenOutput) {
    constexpr auto operations = stateful_sequence();
    const auto result = validate_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.token_count, 4U);
}

TEST(LzssFieldContext, RejectsUnexpectedOperationShapeAtStableIndex) {
    struct Mutation {
        ModeledOperation operation;
        LzssFieldContextError error;
    };
    constexpr std::array mutations{
        Mutation{bypass(1, 0),
                 LzssFieldContextError::unexpected_operation_kind},
        Mutation{symbol(4, 256, 'A'),
                 LzssFieldContextError::unexpected_context},
        Mutation{symbol(3, 255, 'A'),
                 LzssFieldContextError::unexpected_alphabet},
        Mutation{symbol(3, 256, 256),
                 LzssFieldContextError::invalid_symbol},
        Mutation{{ModeledOperationKind::symbol, 3, 256, 'A', 1},
                  LzssFieldContextError::nonzero_unused_field},
    };
    for (const auto& mutation : mutations) {
        auto operations = one_literal();
        operations[1] = mutation.operation;
        const auto result = validate_lzss_field_context_operations(
            operations, {}, literal_context(), marc::core::DecoderLimits{});
        EXPECT_EQ(result.error, mutation.error);
        EXPECT_EQ(result.operation_index, 1U);
        EXPECT_EQ(result.token_index, 0U);
        EXPECT_EQ(result.token_count, 0U);
        EXPECT_EQ(result.raw_size, 0U);
    }
}

TEST(LzssFieldContext, RejectsBypassWidthAndUnusedFields) {
    auto operations = stateful_sequence();
    operations[6].bit_count = 1;
    auto result = validate_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_bypass_width);
    EXPECT_EQ(result.operation_index, 6U);

    operations = stateful_sequence();
    operations[6].context_id = 1;
    result = validate_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::nonzero_unused_field);

    operations = stateful_sequence();
    operations[6].value = 4;
    result = validate_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_symbol);
}

TEST(LzssFieldContext, RejectsReconstructedLengthOutsideVariant) {
    constexpr std::array operations{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 1), symbol(21, 8, 7), bypass(7, 127),
        symbol(30, 17, 0)};
    const auto result = validate_lzss_field_context_operations(
        operations, {}, {2, 6, 12, 260, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_length);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.raw_size, 1U);
}

TEST(LzssFieldContext, RejectsReconstructedDistanceOutsideVariant) {
    constexpr std::array operations{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 1), symbol(21, 8, 0),
        symbol(23, 17, 16), bypass(16, 1)};
    const auto result = validate_lzss_field_context_operations(
        operations, {}, {2, 6, 23, 6, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_index, 1U);
}

TEST(LzssFieldContext, RejectsTruncatedTokenAndTrailingOperations) {
    constexpr std::array truncated{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 1), symbol(21, 8, 2)};
    auto result = validate_lzss_field_context_operations(
        truncated, {}, {2, 4, 4, 11, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::truncated_token);
    EXPECT_EQ(result.operation_index, 4U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.raw_size, 1U);

    constexpr std::array trailing{
        symbol(0, 2, 0), symbol(3, 256, 'A'), symbol(1, 2, 0)};
    result = validate_lzss_field_context_operations(
        trailing, {}, {1, 3, 3, 2, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::trailing_operations);
}

TEST(LzssFieldContext, RejectsDeclaredCountMismatches) {
    constexpr auto operations = one_literal();
    auto result = validate_lzss_field_context_operations(
        operations, {}, {1, 3, 2, 1, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::event_count_mismatch);

    result = validate_lzss_field_context_operations(
        operations, {}, {1, 2, 3, 1, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::decision_count_mismatch);

    result = validate_lzss_field_context_operations(
        operations, {}, {1, 2, 2, 2, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::raw_size_mismatch);
}

TEST(LzssFieldContext, SmallOutputLeavesAllTokensUnchanged) {
    constexpr auto operations = stateful_sequence();
    std::array<LzssTypedToken, 4> output{};
    for (auto& token : output) token.literal = 0xcc;
    const auto result = invert_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{},
        std::span<LzssTypedToken>{output}.first(3));
    EXPECT_EQ(result.error, LzssFieldContextError::output_too_small);
    for (const auto& token : output) EXPECT_EQ(token.literal, 0xcc);
}

TEST(LzssFieldContext, WritesOnlyDeclaredTokenExtent) {
    constexpr auto operations = one_literal();
    std::array<LzssTypedToken, 2> output{};
    output[1].literal = 0xcc;
    const auto result = invert_lzss_field_context_operations(
        operations, {}, literal_context(), marc::core::DecoderLimits{},
        output);
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(output[0].literal, 'A');
    EXPECT_EQ(output[1].literal, 0xcc);
}

TEST(LzssFieldContext, RejectsOperationTokenAliasingAtomically) {
    auto operations = one_literal();
    const auto original = operations;
    auto* token_pointer = reinterpret_cast<LzssTypedToken*>(operations.data());
    const std::span<LzssTypedToken> aliased_output{token_pointer, 1};
    const auto result = invert_lzss_field_context_operations(
        operations, {}, literal_context(), marc::core::DecoderLimits{},
        aliased_output);
    EXPECT_EQ(result.error, LzssFieldContextError::overlapping_buffers);
    EXPECT_EQ(operations[0].kind, original[0].kind);
    EXPECT_EQ(operations[0].context_id, original[0].context_id);
    EXPECT_EQ(operations[0].alphabet_size, original[0].alphabet_size);
    EXPECT_EQ(operations[0].value, original[0].value);
}

TEST(LzssFieldContext, EnforcesOperationStorageAndAggregateOutputLimits) {
    constexpr auto operations = one_literal();
    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = sizeof(ModeledOperation) - 1;
    limits.max_block_size = 1;
    auto result = validate_lzss_field_context_operations(
        operations, {}, literal_context(), limits);
    EXPECT_EQ(result.error, LzssFieldContextError::limit_exceeded);

    limits = {};
    auto context = literal_context();
    context.output_already_committed = limits.max_total_output_size;
    result = validate_lzss_field_context_operations(
        operations, {}, context, limits);
    EXPECT_EQ(result.error, LzssFieldContextError::limit_exceeded);
}

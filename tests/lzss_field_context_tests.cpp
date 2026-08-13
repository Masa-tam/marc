#include "context/lzss_field_context.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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

[[nodiscard]] constexpr auto stateful_tokens() {
    return std::array{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::literal, 'B', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 2, 10},
        LzssTypedToken{LzssTypedTokenKind::literal, 'C', 0, 0}};
}

[[nodiscard]] constexpr LzssFieldContextValidationContext literal_context() {
    return {1, 2, 2, 1, 0};
}

} // namespace

TEST(LzssFieldContextModel, PlansAndMaterializesSpecifiedStatefulVector) {
    constexpr auto tokens = stateful_tokens();
    constexpr auto expected = stateful_sequence();
    constexpr marc::dictionary::internal::LzssTypedFrameValidationContext
        context{4, 13, 0};
    const auto plan = plan_lzss_field_context_operations(
        tokens, {}, context, marc::core::DecoderLimits{});
    ASSERT_EQ(plan.error, LzssFieldContextError::none);
    EXPECT_EQ(plan.operation_count, expected.size());
    EXPECT_EQ(plan.decision_count, 12U);
    EXPECT_EQ(plan.token_count, tokens.size());
    EXPECT_EQ(plan.raw_size, 13U);

    std::array<ModeledOperation, expected.size()> operations{};
    const auto result = model_lzss_field_context_tokens(
        tokens, {}, context, marc::core::DecoderLimits{}, operations);
    ASSERT_EQ(result.error, LzssFieldContextError::none);
    for (std::size_t index = 0; index < operations.size(); ++index) {
        EXPECT_EQ(operations[index].kind, expected[index].kind) << index;
        EXPECT_EQ(operations[index].context_id, expected[index].context_id)
            << index;
        EXPECT_EQ(operations[index].alphabet_size,
                  expected[index].alphabet_size) << index;
        EXPECT_EQ(operations[index].value, expected[index].value) << index;
        EXPECT_EQ(operations[index].bit_count, expected[index].bit_count)
            << index;
    }

    std::array<LzssTypedToken, tokens.size()> reconstructed{};
    const auto inverse = invert_lzss_field_context_operations(
        operations, {}, {4, 11, 12, 13, 0},
        marc::core::DecoderLimits{}, reconstructed);
    ASSERT_EQ(inverse.error, LzssFieldContextError::none);
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(reconstructed[index].kind, tokens[index].kind) << index;
        EXPECT_EQ(reconstructed[index].literal, tokens[index].literal)
            << index;
        EXPECT_EQ(reconstructed[index].distance, tokens[index].distance)
            << index;
        EXPECT_EQ(reconstructed[index].length, tokens[index].length) << index;
    }
}

TEST(LzssFieldContextLayout, SelectsOnlyTheTwoReservedVariantPairs) {
    const auto old_layout = select_lzss_field_context_layout(2, 1, 1);
    ASSERT_EQ(old_layout.error, LzssFieldContextLayoutError::none);
    EXPECT_EQ(old_layout.layout.context_variant,
              LzssFieldContextVariant::field_context_64k);
    EXPECT_EQ(old_layout.layout.frequency_entries, 4518U);
    EXPECT_EQ(old_layout.layout.maximum_bypass_bits, 16U);
    EXPECT_EQ(old_layout.layout.maximum_decisions_per_token, 26U);
    ASSERT_NE(old_layout.layout.alphabets, nullptr);
    EXPECT_EQ((*old_layout.layout.alphabets)[23], 17U);
    EXPECT_EQ(old_layout.layout.offsets->back(), 4518U);

    const auto extended_layout = select_lzss_field_context_layout(3, 1, 2);
    ASSERT_EQ(extended_layout.error, LzssFieldContextLayoutError::none);
    EXPECT_EQ(extended_layout.layout.context_variant,
              LzssFieldContextVariant::field_context_1m);
    EXPECT_EQ(extended_layout.layout.frequency_entries, 4550U);
    EXPECT_EQ(extended_layout.layout.maximum_bypass_bits, 20U);
    EXPECT_EQ(extended_layout.layout.maximum_decisions_per_token, 30U);
    ASSERT_NE(extended_layout.layout.alphabets, nullptr);
    EXPECT_EQ((*extended_layout.layout.alphabets)[23], 21U);
    EXPECT_EQ(extended_layout.layout.offsets->back(), 4550U);

    EXPECT_EQ(select_lzss_field_context_layout(1, 1, 1).error,
              LzssFieldContextLayoutError::unknown_dictionary_variant);
    EXPECT_EQ(select_lzss_field_context_layout(2, 2, 1).error,
              LzssFieldContextLayoutError::unknown_context_algorithm);
    EXPECT_EQ(select_lzss_field_context_layout(2, 1, 3).error,
              LzssFieldContextLayoutError::unsupported_context_variant);
    EXPECT_EQ(select_lzss_field_context_layout(2, 1, 2).error,
              LzssFieldContextLayoutError::incompatible_variants);
    EXPECT_EQ(select_lzss_field_context_layout(3, 1, 1).error,
              LzssFieldContextLayoutError::incompatible_variants);
}

TEST(LzssFieldContextLayout, KeepsVariantOneAliasesByteIdentical) {
    EXPECT_EQ(lzss_field_context_alphabets,
              lzss_field_context_alphabets_v1);
    EXPECT_EQ(lzss_field_context_offsets, lzss_field_context_offsets_v1);
    EXPECT_EQ(lzss_field_context_frequency_entries,
              lzss_field_context_frequency_entries_v1);

    constexpr auto tokens = stateful_tokens();
    constexpr marc::dictionary::internal::LzssTypedFrameValidationContext
        context{4, 13, 0};
    std::array<ModeledOperation, 11> implicit_operations{};
    std::array<ModeledOperation, 11> explicit_operations{};
    ASSERT_EQ(model_lzss_field_context_tokens(
                  tokens, {}, context, marc::core::DecoderLimits{},
                  implicit_operations).error,
              LzssFieldContextError::none);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  tokens, {}, context, marc::core::DecoderLimits{},
                  explicit_operations,
                  LzssFieldContextVariant::field_context_64k).error,
              LzssFieldContextError::none);
    for (std::size_t index = 0; index < implicit_operations.size(); ++index) {
        EXPECT_EQ(implicit_operations[index].kind,
                  explicit_operations[index].kind) << index;
        EXPECT_EQ(implicit_operations[index].context_id,
                  explicit_operations[index].context_id) << index;
        EXPECT_EQ(implicit_operations[index].alphabet_size,
                  explicit_operations[index].alphabet_size) << index;
        EXPECT_EQ(implicit_operations[index].value,
                  explicit_operations[index].value) << index;
        EXPECT_EQ(implicit_operations[index].bit_count,
                  explicit_operations[index].bit_count) << index;
    }
}

TEST(LzssFieldContextLayout, ClassifiesEveryExtendedWindowBoundaryExactly) {
    struct Boundary {
        std::uint32_t distance;
        std::uint8_t expected_class;
    };
    constexpr std::array boundaries{
        Boundary{65535, 15}, Boundary{65536, 16},
        Boundary{65537, 16}, Boundary{131071, 16},
        Boundary{131072, 17}, Boundary{1048575, 19},
        Boundary{1048576, 20}};
    for (const auto& boundary : boundaries) {
        EXPECT_EQ(lzss_field_context_value_class(boundary.distance),
                  boundary.expected_class)
            << "distance=" << boundary.distance;
    }
}

TEST(LzssFieldContextModel, VariantThreeModelsFirstNewDistanceClass) {
    constexpr std::uint32_t distance = 131072;
    std::vector<LzssTypedToken> tokens(distance + 1, {
        LzssTypedTokenKind::literal, 'A', 0, 0});
    tokens.back() = {LzssTypedTokenKind::match, 0, distance, 5};
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = 1048576;
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()), distance + 5, 0};

    const auto plan = plan_lzss_field_context_operations(
        tokens, parameters, context, marc::core::DecoderLimits{},
        LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(plan.error, LzssFieldContextError::none);
    ASSERT_EQ(plan.operation_count, 2U * distance + 4U);
    std::vector<ModeledOperation> operations(plan.operation_count);
    const auto modeled = model_lzss_field_context_tokens(
        tokens, parameters, context, marc::core::DecoderLimits{}, operations,
        LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(modeled.error, LzssFieldContextError::none);

    const auto match_begin = operations.size() - 4;
    const auto distance_symbol = operations[match_begin + 2];
    EXPECT_EQ(distance_symbol.kind, ModeledOperationKind::symbol);
    EXPECT_EQ(distance_symbol.context_id, 23U);
    EXPECT_EQ(distance_symbol.alphabet_size, 21U);
    EXPECT_EQ(distance_symbol.value, 17U);
    EXPECT_EQ(distance_symbol.bit_count, 0U);
    const auto distance_bypass = operations[match_begin + 3];
    EXPECT_EQ(distance_bypass.kind, ModeledOperationKind::bypass_bits);
    EXPECT_EQ(distance_bypass.value, 0U);
    EXPECT_EQ(distance_bypass.bit_count, 17U);

    std::vector<LzssTypedToken> reconstructed(tokens.size());
    const LzssFieldContextValidationContext validation{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(operations.size()),
        modeled.decision_count,
        distance + 5,
        0};
    const auto inverted = invert_lzss_field_context_operations(
        operations, parameters, validation, marc::core::DecoderLimits{},
        reconstructed, LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(inverted.error, LzssFieldContextError::none);
    EXPECT_EQ(reconstructed.back().distance, distance);

    EXPECT_EQ(validate_lzss_field_context_operations(
                  operations, parameters, validation,
                  marc::core::DecoderLimits{},
                  LzssFieldContextVariant::field_context_64k).error,
              LzssFieldContextError::invalid_parameters);
}

TEST(LzssFieldContextModel, AcceptsEmptyFrameWithoutWritingOutput) {
    std::array<ModeledOperation, 1> output{
        symbol(30, 17, 16)};
    const auto before = output;
    const auto result = model_lzss_field_context_tokens(
        {}, {}, {}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 0U);
    EXPECT_EQ(output[0].context_id, before[0].context_id);
    EXPECT_EQ(output[0].value, before[0].value);
}

TEST(LzssFieldContextModel, OmitsZeroWidthBypassOperations) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 5}};
    constexpr std::array expected{
        symbol(0, 2, 0), symbol(3, 256, 'A'),
        symbol(1, 2, 1), symbol(21, 8, 0), symbol(23, 17, 0)};
    std::array<ModeledOperation, expected.size()> output{};
    const auto result = model_lzss_field_context_tokens(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{}, output);
    ASSERT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 5U);
    EXPECT_EQ(result.decision_count, 5U);
    for (std::size_t index = 0; index < output.size(); ++index) {
        EXPECT_EQ(output[index].kind, expected[index].kind) << index;
        EXPECT_EQ(output[index].context_id, expected[index].context_id)
            << index;
        EXPECT_EQ(output[index].alphabet_size,
                  expected[index].alphabet_size) << index;
        EXPECT_EQ(output[index].value, expected[index].value) << index;
        EXPECT_EQ(output[index].bit_count, expected[index].bit_count) << index;
    }
}

TEST(LzssFieldContextModel, EncodesMaximumLengthClassExactly) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 258}};
    std::array<ModeledOperation, 6> output{};
    const auto result = model_lzss_field_context_tokens(
        tokens, {}, {2, 259, 0}, marc::core::DecoderLimits{}, output);
    ASSERT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(result.operation_count, 6U);
    EXPECT_EQ(result.decision_count, 12U);
    EXPECT_EQ(output[3].context_id, 21U);
    EXPECT_EQ(output[3].value, 7U);
    EXPECT_EQ(output[4].kind, ModeledOperationKind::bypass_bits);
    EXPECT_EQ(output[4].bit_count, 7U);
    EXPECT_EQ(output[4].value, 126U);
    EXPECT_EQ(output[5].context_id, 30U);
    EXPECT_EQ(output[5].value, 0U);
}

TEST(LzssFieldContextModel, PropagatesTypedFrameFailures) {
    constexpr std::array invalid{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 2, 5}};
    auto result = plan_lzss_field_context_operations(
        invalid, {}, {2, 6, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    EXPECT_EQ(result.token_error, LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.token_index, 1U);
    EXPECT_EQ(result.raw_size, 1U);

    constexpr std::array literals{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::literal, 'B', 0, 0}};
    result = plan_lzss_field_context_operations(
        literals, {}, {1, 2, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::token_count_mismatch);

    result = plan_lzss_field_context_operations(
        literals, {}, {2, 3, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::raw_size_mismatch);

    result = plan_lzss_field_context_operations(
        literals, {}, {2, 1, 0}, marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::trailing_tokens);

    auto invalid_parameters =
        marc::dictionary::internal::LzssParameters{};
    invalid_parameters.min_match_length = 4;
    result = plan_lzss_field_context_operations(
        literals, invalid_parameters, {2, 2, 0},
        marc::core::DecoderLimits{});
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_parameters);
}

TEST(LzssFieldContextModel, ShortOutputLeavesEveryOperationUnchanged) {
    constexpr auto tokens = stateful_tokens();
    std::array<ModeledOperation, 11> output{};
    for (auto& operation : output) operation.value = 0xCCCCCCCCU;
    const auto result = model_lzss_field_context_tokens(
        tokens, {}, {4, 13, 0}, marc::core::DecoderLimits{},
        std::span<ModeledOperation>{output}.first(10));
    EXPECT_EQ(result.error, LzssFieldContextError::output_too_small);
    for (const auto& operation : output) {
        EXPECT_EQ(operation.value, 0xCCCCCCCCU);
    }
}

TEST(LzssFieldContextModel, WritesOnlyPlannedOperationExtent) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0}};
    std::array<ModeledOperation, 3> output{};
    output[2].value = 0xCCCCCCCCU;
    const auto result = model_lzss_field_context_tokens(
        tokens, {}, {1, 1, 0}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssFieldContextError::none);
    EXPECT_EQ(output[0].value, 0U);
    EXPECT_EQ(output[1].value, static_cast<std::uint32_t>('A'));
    EXPECT_EQ(output[2].value, 0xCCCCCCCCU);
}

TEST(LzssFieldContextModel, RejectsTokenOperationAliasingAtomically) {
    std::array<LzssTypedToken, 3> tokens{};
    tokens[0] = {LzssTypedTokenKind::literal, 'A', 0, 0};
    const auto original = tokens;
    auto* operation_pointer =
        reinterpret_cast<ModeledOperation*>(tokens.data());
    const std::span<ModeledOperation> aliased_output{operation_pointer, 2};
    const auto result = model_lzss_field_context_tokens(
        std::span<const LzssTypedToken>{tokens}.first(1), {}, {1, 1, 0},
        marc::core::DecoderLimits{}, aliased_output);
    EXPECT_EQ(result.error, LzssFieldContextError::overlapping_buffers);
    EXPECT_EQ(tokens[0].kind, original[0].kind);
    EXPECT_EQ(tokens[0].literal, original[0].literal);
    EXPECT_EQ(tokens[0].distance, original[0].distance);
    EXPECT_EQ(tokens[0].length, original[0].length);
}

TEST(LzssFieldContextModel, EnforcesPlannedOperationStorageLimit) {
    constexpr auto tokens = stateful_tokens();
    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes =
        11 * sizeof(ModeledOperation) - 1;
    limits.max_block_size = 13;
    const auto result = plan_lzss_field_context_operations(
        tokens, {}, {4, 13, 0}, limits);
    EXPECT_EQ(result.error, LzssFieldContextError::limit_exceeded);
    EXPECT_EQ(result.operation_count, 11U);
}

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

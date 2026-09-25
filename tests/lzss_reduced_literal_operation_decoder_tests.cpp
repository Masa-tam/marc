#include "context/lzss_reduced_literal_operation_decoder.hpp"
#include "context/lzss_reduced_literal_operations.hpp"
#include "context/lzss_short_length_escape.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssParameters;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

constexpr LzssParameters parameters{65536, 3, 258, 0};
constexpr LzssFieldContextValidationContext hand_context{2, 6, 6, 4, 0};

[[nodiscard]] constexpr std::array<ModeledOperation, 6> hand_operations() {
    return {{
        {ModeledOperationKind::symbol, 0, 2, 0, 0},
        {ModeledOperationKind::symbol, 3, 256, 0x61, 0},
        {ModeledOperationKind::symbol, 1, 2, 1, 0},
        {ModeledOperationKind::symbol, 13, 9, 8, 0},
        {ModeledOperationKind::bypass_bits, 0, 0, 0, 1},
        {ModeledOperationKind::symbol, 23, 17, 0, 0},
    }};
}

TEST(LzssReducedLiteralOperationDecoder, ValidatesAndInvertsHandVector) {
    const auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    const auto checked = validate_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits);
    ASSERT_EQ(checked.error, LzssFieldContextError::none);
    EXPECT_EQ(checked.token_count, 2U);
    EXPECT_EQ(checked.operation_count, 6U);
    EXPECT_EQ(checked.decision_count, 6U);
    EXPECT_EQ(checked.raw_size, 4U);

    std::array<LzssTypedToken, 2> tokens{};
    const auto inverted = invert_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits, tokens);
    ASSERT_EQ(inverted.error, LzssFieldContextError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 0x61U);
    EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
}

TEST(LzssReducedLiteralOperationDecoder, AcceptsFourAndMaximumLength) {
    auto operations = hand_operations();
    operations[4].value = 1;
    auto context = hand_context;
    context.declared_raw_size = 5;
    std::array<LzssTypedToken, 2> tokens{};
    const auto limits = marc::core::DecoderLimits{};
    auto inverted = invert_lzss_reduced_literal_operations(
        operations, parameters, context, limits, tokens);
    ASSERT_EQ(inverted.error, LzssFieldContextError::none);
    EXPECT_EQ(tokens[1].length, 4U);

    operations[3].value = 7;
    operations[4].bit_count = 7;
    operations[4].value = 126;
    operations[5].context_id = 22;
    context.declared_raw_size = 259;
    context.declared_decision_count = 12;
    inverted = invert_lzss_reduced_literal_operations(
        operations, parameters, context, limits, tokens);
    ASSERT_EQ(inverted.error, LzssFieldContextError::none);
    EXPECT_EQ(tokens[1].length, 258U);
}

TEST(LzssReducedLiteralOperationDecoder, InvertsEveryPermittedLength) {
    const auto limits = marc::core::DecoderLimits{};
    for (std::uint32_t length = 3; length <= 258; ++length) {
        auto operations = hand_operations();
        const auto field = encode_lzss_short_length_escape(length);
        ASSERT_EQ(field.error, LzssShortLengthEscapeError::none);
        operations[3].value = field.length_class;
        operations[5].context_id = static_cast<std::uint16_t>(
            15 + field.length_class);
        auto context = hand_context;
        context.declared_raw_size = length + 1;
        context.declared_decision_count = static_cast<std::uint32_t>(
            5 + field.bit_count);
        if (field.bit_count == 0) {
            operations[4] = operations[5];
            context.declared_event_count = 5;
        } else {
            operations[4].bit_count = field.bit_count;
            operations[4].value = field.extra;
        }
        std::array<LzssTypedToken, 2> tokens{};
        const auto result = invert_lzss_reduced_literal_operations(
            std::span{operations}.first(context.declared_event_count),
            parameters, context, limits, tokens);
        ASSERT_EQ(result.error, LzssFieldContextError::none) << length;
        EXPECT_EQ(tokens[1].length, length);
        EXPECT_EQ(tokens[1].distance, 1U);
    }
}

TEST(LzssReducedLiteralOperationDecoder, RejectsMalformedBeforeOutput) {
    auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> output{};
    output[0].literal = 0xa5;
    output[1].literal = 0xa5;
    const auto expect_unchanged = [&] {
        EXPECT_EQ(output[0].literal, 0xa5U);
        EXPECT_EQ(output[1].literal, 0xa5U);
    };

    operations[4].value = 2;
    auto result = invert_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits, output);
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_symbol);
    expect_unchanged();

    operations = hand_operations();
    operations[3].value = 7;
    operations[4].bit_count = 7;
    operations[4].value = 127;
    operations[5].context_id = 22;
    auto context = hand_context;
    context.declared_decision_count = 12;
    context.declared_raw_size = 260;
    result = invert_lzss_reduced_literal_operations(
        operations, parameters, context, limits, output);
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    expect_unchanged();

    operations = hand_operations();
    operations[5].context_id = 15;
    result = invert_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits, output);
    EXPECT_EQ(result.error, LzssFieldContextError::unexpected_context);
    expect_unchanged();

    operations = hand_operations();
    operations[5].value = 1;
    std::array<ModeledOperation, 7> with_distance{};
    for (std::size_t index = 0; index < operations.size(); ++index) {
        with_distance[index] = operations[index];
    }
    with_distance[6] = {ModeledOperationKind::bypass_bits, 0, 0, 0, 1};
    context = {2, 7, 7, 4, 0};
    result = invert_lzss_reduced_literal_operations(
        with_distance, parameters, context, limits, output);
    EXPECT_EQ(result.error, LzssFieldContextError::invalid_token);
    expect_unchanged();
}

TEST(LzssReducedLiteralOperationDecoder, RejectsCountsLimitsAndAliases) {
    auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> output{};
    output[0].literal = 0x77;

    auto context = hand_context;
    context.declared_event_count = 5;
    auto result = validate_lzss_reduced_literal_operations(
        operations, parameters, context, limits);
    EXPECT_EQ(result.error, LzssFieldContextError::event_count_mismatch);
    context = hand_context;
    context.declared_decision_count = 7;
    result = validate_lzss_reduced_literal_operations(
        operations, parameters, context, limits);
    EXPECT_EQ(result.error, LzssFieldContextError::decision_count_mismatch);

    auto strict = limits;
    strict.max_frame_size = 3;
    result = validate_lzss_reduced_literal_operations(
        operations, parameters, hand_context, strict);
    EXPECT_EQ(result.error, LzssFieldContextError::limit_exceeded);

    result = invert_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits,
        std::span<LzssTypedToken>{output}.first(1));
    EXPECT_EQ(result.error, LzssFieldContextError::output_too_small);
    EXPECT_EQ(output[0].literal, 0x77U);

    const auto aliased = std::span<LzssTypedToken>{
        reinterpret_cast<LzssTypedToken*>(operations.data()), 2};
    result = invert_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits, aliased);
    EXPECT_EQ(result.error, LzssFieldContextError::overlapping_buffers);
}

TEST(LzssReducedLiteralOperationDecoder, RejectsTruncationAndRawMismatch) {
    auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    auto context = hand_context;
    context.declared_event_count = 5;
    const auto truncated = validate_lzss_reduced_literal_operations(
        std::span{operations}.first(5), parameters, context, limits);
    EXPECT_EQ(truncated.error, LzssFieldContextError::truncated_token);

    context = hand_context;
    context.declared_raw_size = 5;
    const auto mismatch = validate_lzss_reduced_literal_operations(
        operations, parameters, context, limits);
    EXPECT_EQ(mismatch.error, LzssFieldContextError::raw_size_mismatch);

    operations[3].alphabet_size = 8;
    const auto alphabet = validate_lzss_reduced_literal_operations(
        operations, parameters, hand_context, limits);
    EXPECT_EQ(alphabet.error, LzssFieldContextError::unexpected_alphabet);

    auto invalid_parameters = parameters;
    invalid_parameters.min_match_length = 5;
    const auto invalid = validate_lzss_reduced_literal_operations(
        hand_operations(), invalid_parameters, hand_context, limits);
    EXPECT_EQ(invalid.error, LzssFieldContextError::invalid_parameters);
}

TEST(LzssReducedLiteralOperationDecoder, PreservesLiteralHistoryAcrossMatches) {
    const auto limits = marc::core::DecoderLimits{};
    for (std::uint32_t byte = 0; byte < 256; ++byte) {
        const std::array<LzssTypedToken, 3> input{{
            {LzssTypedTokenKind::literal, static_cast<std::uint8_t>(byte), 0, 0},
            {LzssTypedTokenKind::match, 0, 1, 3},
            {LzssTypedTokenKind::literal, 0xff, 0, 0},
        }};
        std::array<ModeledOperation, 8> operations{};
        const auto mapped = model_lzss_reduced_literal_tokens(
            input, parameters, {3, 5, 0}, limits, operations);
        ASSERT_EQ(mapped.error, LzssFieldContextError::none);
        EXPECT_EQ(operations[7].context_id, 4 + (byte >> 5));
        std::array<LzssTypedToken, 3> output{};
        const LzssFieldContextValidationContext context{3, 8, 8, 5, 0};
        const auto restored = invert_lzss_reduced_literal_operations(
            operations, parameters, context, limits, output);
        ASSERT_EQ(restored.error, LzssFieldContextError::none) << byte;
        for (std::size_t i = 0; i < input.size(); ++i) {
            EXPECT_EQ(output[i].kind, input[i].kind);
            EXPECT_EQ(output[i].literal, input[i].literal);
            EXPECT_EQ(output[i].distance, input[i].distance);
            EXPECT_EQ(output[i].length, input[i].length);
        }
        operations[7].context_id = 3;
        EXPECT_EQ(validate_lzss_reduced_literal_operations(
            operations, parameters, context, limits).error,
            LzssFieldContextError::unexpected_context);
    }
}

} // namespace

#include "context/lzss_short_length_escape_range_tokens.hpp"

#include "context/lzss_short_length_escape.hpp"
#include "context/lzss_short_length_escape_operation_decoder.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssParameters;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualDynamicRangeDescriptor;
using marc::entropy::internal::ContextualDynamicRangeEncodeError;

constexpr LzssParameters parameters{65536, 3, 258, 0};

[[nodiscard]] std::vector<ModeledOperation> operations_for_length(
    const std::uint32_t length, const std::uint32_t distance = 1) {
    const auto field = encode_lzss_short_length_escape(length);
    std::vector<ModeledOperation> operations{
        {ModeledOperationKind::symbol, 0, 2, 0, 0},
        {ModeledOperationKind::symbol, 3, 256, 0x61, 0},
        {ModeledOperationKind::symbol, 1, 2, 1, 0},
        {ModeledOperationKind::symbol, 21, 9, field.length_class, 0},
    };
    if (field.bit_count != 0) {
        operations.push_back({ModeledOperationKind::bypass_bits, 0, 0,
                              field.extra, field.bit_count});
    }
    const auto distance_class = static_cast<std::uint8_t>(
        distance == 1 ? 0 : 1);
    operations.push_back({ModeledOperationKind::symbol,
                          static_cast<std::uint16_t>(23 + field.length_class),
                          17, distance_class, 0});
    if (distance_class != 0) {
        operations.push_back({ModeledOperationKind::bypass_bits, 0, 0,
                              distance - 2, distance_class});
    }
    return operations;
}

[[nodiscard]] LzssFieldContextValidationContext context_for(
    const std::span<const ModeledOperation> operations,
    const std::uint32_t length) {
    std::uint32_t decisions{};
    for (const auto& operation : operations) {
        decisions += operation.kind == ModeledOperationKind::symbol
            ? 1 : operation.bit_count;
    }
    return {2, static_cast<std::uint32_t>(operations.size()),
            decisions, length + 1, 0};
}

[[nodiscard]] bool encode(const std::span<const ModeledOperation> operations,
                          ContextualDynamicRangeDescriptor& descriptor,
                          std::vector<std::byte>& payload) {
    const auto limits = marc::core::DecoderLimits{};
    const auto plan = marc::entropy::internal::
        plan_lzss_short_match_range_operations(operations, limits, descriptor);
    if (plan.error != ContextualDynamicRangeEncodeError::none) return false;
    payload.resize(plan.payload_size);
    const auto encoded = marc::entropy::internal::
        encode_lzss_short_match_range_operations(
            operations, limits, payload, descriptor);
    return encoded.error == ContextualDynamicRangeEncodeError::none;
}

} // namespace

TEST(LzssShortLengthEscapeRangeTokens, DecodesEveryLengthFromRangePayload) {
    const auto limits = marc::core::DecoderLimits{};
    for (std::uint32_t length = 3; length <= 258; ++length) {
        const auto operations = operations_for_length(length);
        const auto context = context_for(operations, length);
        ASSERT_EQ(validate_lzss_short_length_escape_operations(
                      operations, parameters, context, limits).error,
                  LzssFieldContextError::none) << length;
        ContextualDynamicRangeDescriptor descriptor{};
        std::vector<std::byte> payload{};
        ASSERT_TRUE(encode(operations, descriptor, payload)) << length;
        const auto checked = validate_lzss_short_length_escape_range_tokens(
            descriptor, payload, parameters, context, limits);
        ASSERT_EQ(checked.error, LzssContextualRangeDecodeError::none)
            << length;
        std::array<LzssTypedToken, 2> tokens{};
        const auto decoded = decode_lzss_short_length_escape_range_tokens(
            descriptor, payload, parameters, context, limits, tokens);
        ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none)
            << length;
        EXPECT_EQ(decoded.entropy.event_count, context.declared_event_count);
        EXPECT_EQ(decoded.entropy.decision_count,
                  context.declared_decision_count);
        EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
        EXPECT_EQ(tokens[0].literal, 0x61U);
        EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
        EXPECT_EQ(tokens[1].distance, 1U);
        EXPECT_EQ(tokens[1].length, length);
    }
}

TEST(LzssShortLengthEscapeRangeTokens, RejectsForbiddenTerminalLength) {
    auto operations = operations_for_length(258);
    operations[4].value = 127; // Class 7 terminal extra decodes to 259.
    auto context = context_for(operations, 259);
    ContextualDynamicRangeDescriptor descriptor{};
    std::vector<std::byte> payload{};
    ASSERT_TRUE(encode(operations, descriptor, payload));
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    tokens[0].literal = 0xa5;
    tokens[1].literal = 0xa5;
    const auto decoded = decode_lzss_short_length_escape_range_tokens(
        descriptor, payload, parameters, context, limits, tokens);
    EXPECT_EQ(decoded.error, LzssContextualRangeDecodeError::invalid_token);
    EXPECT_EQ(tokens[0].literal, 0xa5U);
    EXPECT_EQ(tokens[1].literal, 0xa5U);
}

TEST(LzssShortLengthEscapeRangeTokens, RejectsCountsPayloadAndOutputAlias) {
    const auto operations = operations_for_length(3);
    const auto context = context_for(operations, 3);
    ContextualDynamicRangeDescriptor descriptor{};
    std::vector<std::byte> payload{};
    ASSERT_TRUE(encode(operations, descriptor, payload));
    const auto limits = marc::core::DecoderLimits{};

    auto wrong = descriptor;
    ++wrong.decision_count;
    EXPECT_EQ(validate_lzss_short_length_escape_range_tokens(
                  wrong, payload, parameters, context, limits).error,
              LzssContextualRangeDecodeError::invalid_counts);

    auto corrupt = payload;
    corrupt[0] = std::byte{1};
    std::array<LzssTypedToken, 2> tokens{};
    tokens[0].literal = 0x77;
    EXPECT_EQ(decode_lzss_short_length_escape_range_tokens(
                  descriptor, corrupt, parameters, context, limits,
                  tokens).error,
              LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(tokens[0].literal, 0x77U);

    EXPECT_EQ(decode_lzss_short_length_escape_range_tokens(
                  descriptor, payload, parameters, context, limits,
                  std::span{tokens}.first(1)).error,
              LzssContextualRangeDecodeError::output_too_small);

    ASSERT_LE(payload.size(), sizeof(tokens));
    std::memcpy(tokens.data(), payload.data(), payload.size());
    const auto alias = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(tokens.data()), payload.size()};
    EXPECT_EQ(decode_lzss_short_length_escape_range_tokens(
                  descriptor, alias, parameters, context, limits,
                  tokens).error,
              LzssContextualRangeDecodeError::overlapping_buffers);
}

TEST(LzssShortLengthEscapeRangeTokens, RejectsInvalidHistoryAndTrailingByte) {
    const auto limits = marc::core::DecoderLimits{};
    const auto operations = operations_for_length(3, 2);
    const auto context = context_for(operations, 3);
    ContextualDynamicRangeDescriptor descriptor{};
    std::vector<std::byte> payload{};
    ASSERT_TRUE(encode(operations, descriptor, payload));
    std::array<LzssTypedToken, 2> tokens{};
    tokens[0].literal = 0x77;
    const auto invalid_history = decode_lzss_short_length_escape_range_tokens(
        descriptor, payload, parameters, context, limits, tokens);
    EXPECT_EQ(invalid_history.error,
              LzssContextualRangeDecodeError::invalid_token);
    EXPECT_EQ(tokens[0].literal, 0x77U);

    const auto valid_operations = operations_for_length(3);
    const auto valid_context = context_for(valid_operations, 3);
    ASSERT_TRUE(encode(valid_operations, descriptor, payload));
    payload.push_back(std::byte{0});
    descriptor.payload_size = static_cast<std::uint32_t>(payload.size());
    const auto trailing = validate_lzss_short_length_escape_range_tokens(
        descriptor, payload, parameters, valid_context, limits);
    EXPECT_EQ(trailing.error, LzssContextualRangeDecodeError::entropy_error);
    EXPECT_EQ(trailing.entropy.error,
              marc::entropy::internal::
                  ContextualDynamicRangeDecodeError::trailing_payload);
}

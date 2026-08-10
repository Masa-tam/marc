#include "context/lzss_contextual_tans_encoder.hpp"

#include "context/lzss_contextual_tans_decoder.hpp"
#include "context/lzss_field_context.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedFrameValidationContext;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualTansDescriptor;
using marc::entropy::internal::TansDecodeEntry;
using marc::entropy::internal::contextual_tans_decode_table_entries;
using marc::entropy::internal::contextual_tans_encode_table_entries;

[[nodiscard]] constexpr auto literal_tokens() {
    return std::array{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0}};
}

[[nodiscard]] constexpr auto mixed_tokens() {
    return std::array{
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::literal, 'B', 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 2, 6},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 5},
        LzssTypedToken{LzssTypedTokenKind::literal, 'C', 0, 0},
    };
}

[[nodiscard]] std::vector<std::uint16_t> encode_tables() {
    return std::vector<std::uint16_t>(
        contextual_tans_encode_table_entries);
}

[[nodiscard]] std::vector<TansDecodeEntry> decode_tables() {
    return std::vector<TansDecodeEntry>(
        contextual_tans_decode_table_entries);
}

void expect_descriptor_equal(
    const ContextualTansDescriptor& left,
    const ContextualTansDescriptor& right) {
    EXPECT_EQ(left.decision_count, right.decision_count);
    EXPECT_EQ(left.payload_size, right.payload_size);
    EXPECT_EQ(left.table_log, right.table_log);
    EXPECT_EQ(left.final_valid_bits, right.final_valid_bits);
    EXPECT_EQ(left.flags, right.flags);
    EXPECT_EQ(left.context_count, right.context_count);
    EXPECT_EQ(left.frequency_entry_count, right.frequency_entry_count);
    EXPECT_EQ(left.frequencies, right.frequencies);
}

} // namespace

TEST(LzssContextualTansEncoder, EncodesDocumentedLiteralWithoutOperations) {
    constexpr auto tokens = literal_tokens();
    constexpr LzssTypedFrameValidationContext context{1, 1, 0};
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    auto result = plan_lzss_contextual_tans_tokens(
        tokens, {}, context, {}, tables, descriptor);
    ASSERT_EQ(result.error, LzssContextualTansEncodeError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(result.required_table_entries,
              contextual_tans_encode_table_entries);
    EXPECT_EQ(descriptor.frequencies[0], 4096U);
    EXPECT_EQ(descriptor.frequencies[71], 4096U);
    EXPECT_EQ(descriptor.final_valid_bits, 0U);

    std::array payload{std::byte{0xcc}, std::byte{0xcc}, std::byte{0xcc}};
    descriptor = {};
    result = encode_lzss_contextual_tans_tokens(
        tokens, {}, context, {}, tables, payload, descriptor);
    ASSERT_EQ(result.error, LzssContextualTansEncodeError::none);
    EXPECT_EQ(payload[0], std::byte{0});
    EXPECT_EQ(payload[1], std::byte{0});
    EXPECT_EQ(payload[2], std::byte{0xcc});
}

TEST(LzssContextualTansEncoder, MatchesMaterializedOperationReference) {
    constexpr auto tokens = mixed_tokens();
    constexpr LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()), 14, 0};
    const auto operation_plan = plan_lzss_field_context_operations(
        tokens, {}, context, {});
    ASSERT_EQ(operation_plan.error, LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(operation_plan.operation_count);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  tokens, {}, context, {}, operations).error,
              LzssFieldContextError::none);

    auto reference_tables = encode_tables();
    ContextualTansDescriptor reference_descriptor{};
    const auto reference_plan =
        marc::entropy::internal::plan_contextual_tans_operations(
            operations, {}, reference_tables, reference_descriptor);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::ContextualTansEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_contextual_tans_operations(
                  operations, {}, reference_tables, reference_payload,
                  reference_descriptor).error,
              marc::entropy::internal::ContextualTansEncodeError::none);

    auto direct_tables = encode_tables();
    ContextualTansDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_tans_tokens(
        tokens, {}, context, {}, direct_tables, direct_descriptor);
    ASSERT_EQ(direct_plan.error, LzssContextualTansEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());
    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_tans_tokens(
                  tokens, {}, context, {}, direct_tables, direct_payload,
                  direct_descriptor).error,
              LzssContextualTansEncodeError::none);
    expect_descriptor_equal(direct_descriptor, reference_descriptor);
    EXPECT_EQ(direct_payload, reference_payload);
}

TEST(LzssContextualTansEncoder, DirectPayloadDecodesToMixedTokens) {
    constexpr auto expected = mixed_tokens();
    constexpr LzssTypedFrameValidationContext frame_context{
        static_cast<std::uint32_t>(expected.size()), 14, 0};
    auto encode_table_storage = encode_tables();
    ContextualTansDescriptor descriptor{};
    const auto plan = plan_lzss_contextual_tans_tokens(
        expected, {}, frame_context, {}, encode_table_storage, descriptor);
    ASSERT_EQ(plan.error, LzssContextualTansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_tans_tokens(
                  expected, {}, frame_context, {}, encode_table_storage,
                  payload, descriptor).error,
              LzssContextualTansEncodeError::none);

    auto decode_table_storage = decode_tables();
    std::array<LzssTypedToken, expected.size()> decoded{};
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(plan.event_count), plan.decision_count,
        14, 0};
    const auto result = decode_lzss_contextual_tans_tokens(
        descriptor, payload, {}, decode_context, {}, decode_table_storage,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansDecodeError::none);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(decoded[index].kind, expected[index].kind);
        EXPECT_EQ(decoded[index].literal, expected[index].literal);
        EXPECT_EQ(decoded[index].distance, expected[index].distance);
        EXPECT_EQ(decoded[index].length, expected[index].length);
    }
}

TEST(LzssContextualTansEncoder, PrewriteFailuresPreserveOutputs) {
    constexpr auto tokens = literal_tokens();
    constexpr LzssTypedFrameValidationContext context{1, 1, 0};
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 123;
    std::array payload{std::byte{0xcc}, std::byte{0xcc}};
    auto result = encode_lzss_contextual_tans_tokens(
        tokens, {}, context, {}, tables,
        std::span<std::byte>{payload}.first(1), descriptor);
    EXPECT_EQ(result.error,
              LzssContextualTansEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xcc};
    }));
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<LzssTypedToken, 1> token_storage{tokens[0]};
    auto token_bytes = std::as_writable_bytes(std::span{token_storage});
    const auto before = std::vector<std::byte>(
        token_bytes.begin(), token_bytes.end());
    result = encode_lzss_contextual_tans_tokens(
        token_storage, {}, context, {}, tables, token_bytes.first(2),
        descriptor);
    EXPECT_EQ(result.error,
              LzssContextualTansEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(before, token_bytes));
    EXPECT_EQ(descriptor.decision_count, 123U);

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    result = encode_lzss_contextual_tans_tokens(
        tokens, {}, context, {}, tables, table_bytes.first(2), descriptor);
    EXPECT_EQ(result.error,
              LzssContextualTansEncodeError::overlapping_buffers);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

TEST(LzssContextualTansEncoder, RejectsInvalidTokenFrameAndLimits) {
    auto tokens = literal_tokens();
    tokens[0].distance = 1;
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 123;
    auto result = plan_lzss_contextual_tans_tokens(
        tokens, {}, {1, 1, 0}, {}, tables, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualTansEncodeError::token_validation_error);
    EXPECT_EQ(descriptor.decision_count, 123U);

    tokens = literal_tokens();
    result = plan_lzss_contextual_tans_tokens(
        tokens, {}, {1, 1, 0}, {},
        std::span<std::uint16_t>{tables}.first(tables.size() - 1),
        descriptor);
    EXPECT_EQ(result.error,
              LzssContextualTansEncodeError::table_output_too_small);

    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_tans_encode_table_entries - 1;
    result = plan_lzss_contextual_tans_tokens(
        tokens, {}, {1, 1, 0}, limits, tables, descriptor);
    EXPECT_EQ(result.error, LzssContextualTansEncodeError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    result = plan_lzss_contextual_tans_tokens(
        tokens, {}, {1, 1, 0}, limits, tables, descriptor);
    EXPECT_EQ(result.error, LzssContextualTansEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

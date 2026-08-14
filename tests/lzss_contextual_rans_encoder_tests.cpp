#include "context/lzss_contextual_rans_encoder.hpp"

#include "context/lzss_contextual_rans_decoder.hpp"
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
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

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

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

void expect_descriptor_equal(
    const ContextualRansDescriptor& left,
    const ContextualRansDescriptor& right) {
    EXPECT_EQ(left.decision_count, right.decision_count);
    EXPECT_EQ(left.payload_size, right.payload_size);
    EXPECT_EQ(left.table_log, right.table_log);
    EXPECT_EQ(left.context_count, right.context_count);
    EXPECT_EQ(left.frequency_entry_count, right.frequency_entry_count);
    EXPECT_EQ(left.frequencies, right.frequencies);
}

} // namespace

TEST(LzssContextualRansEncoder, EncodesDocumentedLiteralWithoutOperations) {
    constexpr auto tokens = literal_tokens();
    constexpr LzssTypedFrameValidationContext context{1, 1, 0};
    ContextualRansDescriptor descriptor{};
    auto result = plan_lzss_contextual_rans_tokens(
        tokens, {}, context, {}, descriptor);
    ASSERT_EQ(result.error, LzssContextualRansEncodeError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(descriptor.frequencies[0], 4096U);
    EXPECT_EQ(descriptor.frequencies[71], 4096U);

    std::array<std::byte, 9> payload{};
    payload.back() = std::byte{0xcc};
    descriptor = {};
    result = encode_lzss_contextual_rans_tokens(
        tokens, {}, context, {}, payload, descriptor);
    ASSERT_EQ(result.error, LzssContextualRansEncodeError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        expected, std::span<const std::byte>{payload}.first(expected.size())));
    EXPECT_EQ(payload.back(), std::byte{0xcc});
}

TEST(LzssContextualRansEncoder, MatchesMaterializedOperationReference) {
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

    ContextualRansDescriptor reference_descriptor{};
    const auto reference_plan =
        marc::entropy::internal::plan_contextual_rans_operations(
            operations, {}, reference_descriptor);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::ContextualRansEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_contextual_rans_operations(
                  operations, {}, reference_payload,
                  reference_descriptor).error,
              marc::entropy::internal::ContextualRansEncodeError::none);

    ContextualRansDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_rans_tokens(
        tokens, {}, context, {}, direct_descriptor);
    ASSERT_EQ(direct_plan.error, LzssContextualRansEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());
    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_rans_tokens(
                  tokens, {}, context, {}, direct_payload,
                  direct_descriptor).error,
              LzssContextualRansEncodeError::none);
    expect_descriptor_equal(direct_descriptor, reference_descriptor);
    EXPECT_EQ(direct_payload, reference_payload);
}

TEST(LzssContextualRansEncoder, ExtendedDistanceMatchesReferenceAndDecodes) {
    constexpr std::uint32_t distance = 131072;
    std::vector<LzssTypedToken> expected(
        distance + 1,
        LzssTypedToken{LzssTypedTokenKind::literal, 'A', 0, 0});
    expected.back() = {LzssTypedTokenKind::match, 0, distance, 5};
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = 1048576;
    const LzssTypedFrameValidationContext frame_context{
        static_cast<std::uint32_t>(expected.size()), distance + 5, 0};
    constexpr auto extended = LzssFieldContextVariant::field_context_1m;

    const auto operation_plan = plan_lzss_field_context_operations(
        expected, parameters, frame_context, {}, extended);
    ASSERT_EQ(operation_plan.error, LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(operation_plan.operation_count);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  expected, parameters, frame_context, {}, operations,
                  extended).error,
              LzssFieldContextError::none);
    ASSERT_EQ(operations[operations.size() - 2].alphabet_size, 21U);
    ASSERT_EQ(operations[operations.size() - 2].value, 17U);
    ASSERT_EQ(operations.back().bit_count, 17U);

    ContextualRansDescriptor reference_descriptor{};
    const auto reference_plan =
        marc::entropy::internal::plan_contextual_rans_operations(
            operations, {}, reference_descriptor, extended);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::ContextualRansEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::encode_contextual_rans_operations(
                  operations, {}, reference_payload, reference_descriptor,
                  extended).error,
              marc::entropy::internal::ContextualRansEncodeError::none);

    ContextualRansDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_rans_tokens(
        expected, parameters, frame_context, {}, direct_descriptor, extended);
    ASSERT_EQ(direct_plan.error, LzssContextualRansEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());
    EXPECT_EQ(direct_descriptor.frequency_entry_count,
              lzss_field_context_frequency_entries_v2);
    expect_descriptor_equal(direct_descriptor, reference_descriptor);

    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_rans_tokens(
                  expected, parameters, frame_context, {}, direct_payload,
                  direct_descriptor, extended).error,
              LzssContextualRansEncodeError::none);
    EXPECT_EQ(direct_payload, reference_payload);

    std::vector<std::byte> serialized(direct_plan.descriptor_size);
    std::size_t written{};
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  direct_descriptor, direct_plan.decision_count,
                  static_cast<std::uint32_t>(direct_plan.payload_size), {},
                  serialized, written, extended),
              marc::entropy::internal::ContextualRansFormatError::none);
    ASSERT_EQ(written, serialized.size());
    auto table_storage = tables();
    std::vector<LzssTypedToken> decoded(expected.size());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(direct_plan.event_count),
        direct_plan.decision_count, distance + 5, 0};
    const auto decoded_result = decode_lzss_contextual_rans_tokens(
        serialized, direct_payload, parameters, decode_context, {},
        table_storage, decoded, extended);
    ASSERT_EQ(decoded_result.format_error,
              marc::entropy::internal::ContextualRansFormatError::none);
    ASSERT_EQ(decoded_result.decode.error,
              LzssContextualRansDecodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        expected, decoded, [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    ContextualRansDescriptor sentinel{};
    sentinel.decision_count = 0xa5a5;
    EXPECT_EQ(plan_lzss_contextual_rans_tokens(
                  expected, parameters, frame_context, {}, sentinel).error,
              LzssContextualRansEncodeError::token_validation_error);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);
    EXPECT_EQ(plan_lzss_contextual_rans_tokens(
                  expected, parameters, frame_context, {}, sentinel,
                  static_cast<LzssFieldContextVariant>(99)).error,
              LzssContextualRansEncodeError::token_validation_error);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);

    table_storage = tables();
    std::ranges::fill(decoded, LzssTypedToken{
        LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU});
    const auto crossed = decode_lzss_contextual_rans_tokens(
        serialized, direct_payload, {}, decode_context, {}, table_storage,
        decoded);
    EXPECT_EQ(crossed.format_error,
              marc::entropy::internal::ContextualRansFormatError::
                  invalid_frequency_entry_count);
    EXPECT_EQ(crossed.decode.error,
              LzssContextualRansDecodeError::entropy_error);
    EXPECT_TRUE(std::ranges::all_of(decoded, [](const auto& token) {
        return token.literal == 0xcc && token.distance == 0xccccccccU
            && token.length == 0xccccccccU;
    }));

    const auto unsupported = decode_lzss_contextual_rans_tokens(
        serialized, direct_payload, parameters, decode_context, {},
        table_storage, decoded, static_cast<LzssFieldContextVariant>(99));
    EXPECT_EQ(unsupported.format_error,
              marc::entropy::internal::ContextualRansFormatError::none);
    EXPECT_EQ(unsupported.decode.error,
              LzssContextualRansDecodeError::invalid_parameters);
    EXPECT_TRUE(std::ranges::all_of(decoded, [](const auto& token) {
        return token.literal == 0xcc && token.distance == 0xccccccccU
            && token.length == 0xccccccccU;
    }));
}

TEST(LzssContextualRansEncoder, DirectPayloadDecodesToMixedTokens) {
    constexpr auto expected = mixed_tokens();
    constexpr LzssTypedFrameValidationContext frame_context{
        static_cast<std::uint32_t>(expected.size()), 14, 0};
    ContextualRansDescriptor descriptor{};
    const auto plan = plan_lzss_contextual_rans_tokens(
        expected, {}, frame_context, {}, descriptor);
    ASSERT_EQ(plan.error, LzssContextualRansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_rans_tokens(
                  expected, {}, frame_context, {}, payload, descriptor).error,
              LzssContextualRansEncodeError::none);

    std::vector<std::byte> serialized_descriptor(plan.descriptor_size);
    std::size_t descriptor_written{};
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, plan.decision_count,
                  static_cast<std::uint32_t>(plan.payload_size), {},
                  serialized_descriptor, descriptor_written),
              marc::entropy::internal::ContextualRansFormatError::none);
    ASSERT_EQ(descriptor_written, serialized_descriptor.size());

    auto table_storage = tables();
    std::array<LzssTypedToken, expected.size()> decoded{};
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(plan.event_count), plan.decision_count,
        14, 0};
    const auto result = decode_lzss_contextual_rans_tokens(
        serialized_descriptor, payload, {}, decode_context, {}, table_storage,
        decoded);
    ASSERT_EQ(result.format_error,
              marc::entropy::internal::ContextualRansFormatError::none);
    ASSERT_EQ(result.decode.error, LzssContextualRansDecodeError::none);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(decoded[index].kind, expected[index].kind);
        EXPECT_EQ(decoded[index].literal, expected[index].literal);
        EXPECT_EQ(decoded[index].distance, expected[index].distance);
        EXPECT_EQ(decoded[index].length, expected[index].length);
    }
}

TEST(LzssContextualRansEncoder, PrewriteFailuresPreserveDescriptorAndPayload) {
    constexpr auto tokens = literal_tokens();
    constexpr LzssTypedFrameValidationContext context{1, 1, 0};
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 123;
    std::array<std::byte, 8> payload{};
    payload.fill(std::byte{0xcc});
    auto result = encode_lzss_contextual_rans_tokens(
        tokens, {}, context, {}, std::span<std::byte>{payload}.first(7),
        descriptor);
    EXPECT_EQ(result.error,
              LzssContextualRansEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xcc};
    }));
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<LzssTypedToken, 1> storage{tokens[0]};
    auto bytes = std::as_writable_bytes(std::span{storage});
    const auto before = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7]};
    result = encode_lzss_contextual_rans_tokens(
        storage, {}, context, {}, bytes.first(8), descriptor);
    EXPECT_EQ(result.error,
              LzssContextualRansEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
    EXPECT_EQ(descriptor.decision_count, 123U);
}

TEST(LzssContextualRansEncoder, RejectsInvalidTokenFrameAndLimits) {
    auto tokens = literal_tokens();
    tokens[0].distance = 1;
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 123;
    auto result = plan_lzss_contextual_rans_tokens(
        tokens, {}, {1, 1, 0}, {}, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualRansEncodeError::token_validation_error);
    EXPECT_EQ(descriptor.decision_count, 123U);

    tokens = literal_tokens();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_rans_decode_table_entries - 1;
    result = plan_lzss_contextual_rans_tokens(
        tokens, {}, {1, 1, 0}, limits, descriptor);
    EXPECT_EQ(result.error, LzssContextualRansEncodeError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = sizeof(LzssTypedToken) - 1;
    result = plan_lzss_contextual_rans_tokens(
        tokens, {}, {1, 1, 0}, limits, descriptor);
    EXPECT_EQ(result.error, LzssContextualRansEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

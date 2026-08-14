#include "entropy/contextual_tans_encoder.hpp"

#include "entropy/contextual_tans_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using namespace marc::entropy::internal;

[[nodiscard]] constexpr auto literal_operations() {
    return std::array{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 65, 0},
    };
}

[[nodiscard]] constexpr auto bypass_operations() {
    return std::array{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 2, 2},
    };
}

[[nodiscard]] constexpr auto extended_operations() {
    return std::array{
        ModeledOperation{ModeledOperationKind::symbol, 23, 21, 20, 0},
        ModeledOperation{
            ModeledOperationKind::bypass_bits, 0, 0, 0xabcde, 20},
    };
}

[[nodiscard]] std::vector<std::uint16_t> encode_tables() {
    return std::vector<std::uint16_t>(contextual_tans_encode_table_entries);
}

[[nodiscard]] std::vector<TansDecodeEntry> decode_tables() {
    return std::vector<TansDecodeEntry>(contextual_tans_decode_table_entries);
}

} // namespace

TEST(ContextualTansEncoder, PlansAndEncodesDocumentedLiteralVector) {
    constexpr auto operations = literal_operations();
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    auto result = plan_contextual_tans_operations(
        operations, {}, tables, descriptor);
    ASSERT_EQ(result.error, ContextualTansEncodeError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(result.required_table_entries,
              contextual_tans_encode_table_entries);
    EXPECT_EQ(descriptor.frequencies[0], 4096U);
    EXPECT_EQ(descriptor.frequencies[71], 4096U);
    EXPECT_EQ(descriptor.final_valid_bits, 0U);

    std::array payload{std::byte{0xcc}, std::byte{0xcc}, std::byte{0xcc}};
    descriptor = {};
    result = encode_contextual_tans_operations(
        operations, {}, tables, payload, descriptor);
    ASSERT_EQ(result.error, ContextualTansEncodeError::none);
    EXPECT_EQ(payload[0], std::byte{0});
    EXPECT_EQ(payload[1], std::byte{0});
    EXPECT_EQ(payload[2], std::byte{0xcc});

    auto decoder_tables = decode_tables();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, std::span{payload}.first<2>(), {},
                  decoder_tables).error,
              ContextualTansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(decoder.finish(2, 2).error, ContextualTansDecodeError::none);
}

TEST(ContextualTansEncoder, EncodesAndDecodesLsbFirstBypassVector) {
    constexpr auto operations = bypass_operations();
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    const auto plan = plan_contextual_tans_operations(
        operations, {}, tables, descriptor);
    ASSERT_EQ(plan.error, ContextualTansEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    const auto result = encode_contextual_tans_operations(
        operations, {}, tables, payload, descriptor);
    ASSERT_EQ(result.error, ContextualTansEncodeError::none);
    EXPECT_EQ(descriptor.decision_count, 3U);
    EXPECT_EQ(descriptor.frequencies[1], 4096U);
    EXPECT_EQ(descriptor.final_valid_bits, 2U);
    EXPECT_EQ(result.payload_size, 3U);
    EXPECT_EQ(payload, (std::vector{
        std::byte{0x06}, std::byte{0x00}, std::byte{0x00}}));

    auto decoder_tables = decode_tables();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, decoder_tables).error,
              ContextualTansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 2U);
    EXPECT_EQ(decoder.finish(2, 3).error, ContextualTansDecodeError::none);
}

TEST(ContextualTansEncoder, SelectedLayoutEncodesClassTwentyAndBypassTwenty) {
    constexpr auto operations = extended_operations();
    constexpr auto variant = LzssFieldContextVariant::field_context_1m;
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    const auto plan = plan_contextual_tans_operations(
        operations, {}, tables, descriptor, variant);
    ASSERT_EQ(plan.error, ContextualTansEncodeError::none);
    EXPECT_EQ(plan.decision_count, 21U);
    EXPECT_EQ(descriptor.frequency_entry_count,
              lzss_field_context_frequency_entries_v2);
    const auto offset = lzss_field_context_offsets_v2[23];
    EXPECT_EQ(descriptor.frequencies[offset + 20], 4096U);
    EXPECT_EQ(plan.required_table_entries,
              contextual_tans_encode_table_entries);

    std::vector<std::byte> payload(plan.payload_size);
    const auto encoded = encode_contextual_tans_operations(
        operations, {}, tables, payload, descriptor, variant);
    ASSERT_EQ(encoded.error, ContextualTansEncodeError::none);
    EXPECT_EQ(encoded.payload_size, plan.payload_size);

    auto decoder_tables = decode_tables();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(
                  descriptor, payload, {}, decoder_tables, variant).error,
              ContextualTansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(23, 21, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 20U);
    ASSERT_EQ(decoder.decode_bypass(20, value).error,
              ContextualTansDecodeError::none);
    EXPECT_EQ(value, 0xabcdeU);
    EXPECT_EQ(decoder.finish(2, 21).error,
              ContextualTansDecodeError::none);

    ContextualTansDescriptor sentinel{};
    sentinel.decision_count = 0xa5a5;
    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, {}, tables, sentinel).error,
              ContextualTansEncodeError::invalid_alphabet);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);
    constexpr std::array crossed{
        ModeledOperation{ModeledOperationKind::symbol, 23, 17, 16, 0}};
    EXPECT_EQ(plan_contextual_tans_operations(
                  crossed, {}, tables, sentinel, variant).error,
              ContextualTansEncodeError::invalid_alphabet);
    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, {}, tables, sentinel,
                  static_cast<LzssFieldContextVariant>(0xff)).error,
              ContextualTansEncodeError::unsupported_context_variant);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);
}

TEST(ContextualTansEncoder, NormalizesAndRoundTripsAlternatingContext) {
    std::vector<ModeledOperation> operations;
    operations.reserve(512);
    for (std::uint32_t index = 0; index < 512; ++index) {
        operations.push_back(
            {ModeledOperationKind::symbol, 0, 2, index & 1U, 0});
    }
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    const auto plan = plan_contextual_tans_operations(
        operations, {}, tables, descriptor);
    ASSERT_EQ(plan.error, ContextualTansEncodeError::none);
    EXPECT_EQ(descriptor.frequencies[0], 2048U);
    EXPECT_EQ(descriptor.frequencies[1], 2048U);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_contextual_tans_operations(
                  operations, {}, tables, payload, descriptor).error,
              ContextualTansEncodeError::none);

    auto duplicate_tables = encode_tables();
    auto duplicate_descriptor = ContextualTansDescriptor{};
    auto duplicate_payload = std::vector<std::byte>(plan.payload_size);
    ASSERT_EQ(encode_contextual_tans_operations(
                  operations, {}, duplicate_tables, duplicate_payload,
                  duplicate_descriptor).error,
              ContextualTansEncodeError::none);
    EXPECT_EQ(duplicate_payload, payload);
    EXPECT_EQ(duplicate_descriptor.frequencies, descriptor.frequencies);
    EXPECT_EQ(duplicate_descriptor.final_valid_bits,
              descriptor.final_valid_bits);

    auto decoder_tables = decode_tables();
    ContextualTansDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, decoder_tables).error,
              ContextualTansDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{};
        ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
                  ContextualTansDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    EXPECT_EQ(decoder.finish(512, 512).error,
              ContextualTansDecodeError::none);
}

TEST(ContextualTansEncoder, UsesDeterministicNumericNormalizationTies) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 2, 0},
    };
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    ASSERT_EQ(plan_contextual_tans_operations(
                  operations, {}, tables, descriptor).error,
              ContextualTansEncodeError::none);
    const auto offset = lzss_field_context_offsets[3];
    EXPECT_EQ(descriptor.frequencies[offset], 1366U);
    EXPECT_EQ(descriptor.frequencies[offset + 1], 1365U);
    EXPECT_EQ(descriptor.frequencies[offset + 2], 1365U);
}

TEST(ContextualTansEncoder, EncodeTableBuilderPublishesAtomically) {
    constexpr auto operations = literal_operations();
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    ASSERT_EQ(plan_contextual_tans_operations(
                  operations, {}, tables, descriptor).error,
              ContextualTansEncodeError::none);
    std::ranges::fill(tables, UINT16_C(0xa5a5));

    auto invalid = descriptor;
    invalid.frequencies[0] = 4095;
    EXPECT_EQ(build_contextual_tans_encode_tables(
                  invalid, {}, tables),
              ContextualTansEncodeError::table_error);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto value) {
        return value == UINT16_C(0xa5a5);
    }));

    EXPECT_EQ(build_contextual_tans_encode_tables(
                  descriptor, {},
                  std::span<std::uint16_t>{tables}.first(tables.size() - 1)),
              ContextualTansEncodeError::table_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(tables, [](const auto value) {
        return value == UINT16_C(0xa5a5);
    }));
}

TEST(ContextualTansEncoder, RejectsMalformedOperationsAtStableIndex) {
    struct Case {
        ModeledOperation operation;
        ContextualTansEncodeError error;
    };
    constexpr std::array cases{
        Case{ModeledOperation{static_cast<ModeledOperationKind>(2), 0, 2, 0, 0},
             ContextualTansEncodeError::invalid_operation_kind},
        Case{ModeledOperation{ModeledOperationKind::symbol, 31, 2, 0, 0},
             ContextualTansEncodeError::invalid_context},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 3, 0, 0},
             ContextualTansEncodeError::invalid_alphabet},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 2, 0},
             ContextualTansEncodeError::invalid_symbol},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 1},
             ContextualTansEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 1, 0, 0, 1},
             ContextualTansEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 0},
             ContextualTansEncodeError::invalid_bypass_width},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 4, 2},
             ContextualTansEncodeError::nonzero_unused_field},
    };
    auto tables = encode_tables();
    for (const auto& test : cases) {
        constexpr auto prefix = literal_operations();
        std::array operations{prefix[0], test.operation};
        ContextualTansDescriptor descriptor{};
        descriptor.decision_count = 123;
        const auto result = plan_contextual_tans_operations(
            operations, {}, tables, descriptor);
        EXPECT_EQ(result.error, test.error);
        EXPECT_EQ(result.operation_index, 1U);
        EXPECT_EQ(result.operation_count, 1U);
        EXPECT_EQ(descriptor.decision_count, 123U);
    }
}

TEST(ContextualTansEncoder, PrewriteFailuresPreservePayloadAndDescriptor) {
    constexpr auto operations = literal_operations();
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 123;
    std::array<std::byte, 2> payload{};
    payload.fill(std::byte{0xcc});
    auto result = encode_contextual_tans_operations(
        operations, {}, tables, std::span{payload}.first(1), descriptor);
    EXPECT_EQ(result.error,
              ContextualTansEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xcc};
    }));
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<ModeledOperation, 1> operation_storage{operations[0]};
    auto operation_bytes =
        std::as_writable_bytes(std::span{operation_storage});
    const auto operation_before = operation_bytes[0];
    result = encode_contextual_tans_operations(
        operation_storage, {}, tables, operation_bytes.first(2), descriptor);
    EXPECT_EQ(result.error, ContextualTansEncodeError::overlapping_buffers);
    EXPECT_EQ(operation_bytes[0], operation_before);
    EXPECT_EQ(descriptor.decision_count, 123U);

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    table_bytes[0] = std::byte{0xcc};
    result = encode_contextual_tans_operations(
        operations, {}, tables, table_bytes.first(2), descriptor);
    EXPECT_EQ(result.error, ContextualTansEncodeError::overlapping_buffers);
    EXPECT_EQ(table_bytes[0], std::byte{0xcc});
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<ModeledOperation, 1> shared{operations[0]};
    const std::span<std::uint16_t> aliased_tables{
        &shared[0].context_id, 1};
    result = plan_contextual_tans_operations(
        shared, {}, aliased_tables, descriptor);
    EXPECT_EQ(result.error, ContextualTansEncodeError::overlapping_buffers);
    EXPECT_EQ(shared[0].kind, ModeledOperationKind::symbol);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

TEST(ContextualTansEncoder, EnforcesEmptyStorageAndLocalLimits) {
    auto tables = encode_tables();
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 123;
    EXPECT_EQ(plan_contextual_tans_operations(
                  {}, {}, tables, descriptor).error,
              ContextualTansEncodeError::empty_operations);
    constexpr auto operations = literal_operations();

    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, {},
                  std::span<std::uint16_t>{tables}.first(tables.size() - 1),
                  descriptor).error,
              ContextualTansEncodeError::table_output_too_small);

    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_tans_encode_table_entries - 1;
    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, limits, tables, descriptor).error,
              ContextualTansEncodeError::limit_exceeded);

    limits = {};
    limits.max_block_size = 1;
    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, limits, tables, descriptor).error,
              ContextualTansEncodeError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 1;
    EXPECT_EQ(plan_contextual_tans_operations(
                  operations, limits, tables, descriptor).error,
              ContextualTansEncodeError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 2;
    EXPECT_EQ(plan_contextual_tans_operations(
                  bypass_operations(), limits, tables, descriptor).error,
              ContextualTansEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

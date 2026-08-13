#include "entropy/contextual_dynamic_range_encoder.hpp"

#include "entropy/contextual_dynamic_range_decoder.hpp"

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
        ModeledOperation{ModeledOperationKind::symbol, 20, 8, 2, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 2, 2},
        ModeledOperation{ModeledOperationKind::symbol, 25, 17, 1, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 1},
    };
}

} // namespace

TEST(ContextualDynamicRangeEncoder, PlansAndEncodesOneLiteralVector) {
    constexpr auto operations = literal_operations();
    ContextualDynamicRangeDescriptor descriptor{};
    auto result = plan_contextual_dynamic_range_operations(
        operations, {}, descriptor);
    ASSERT_EQ(result.error, ContextualDynamicRangeEncodeError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 6U);
    EXPECT_EQ(descriptor.decision_count, 2U);
    EXPECT_EQ(descriptor.payload_size, 6U);
    EXPECT_EQ(descriptor.context_count, 31U);

    std::array<std::byte, 7> payload{};
    payload.back() = std::byte{0xCC};
    descriptor = {};
    result = encode_contextual_dynamic_range_operations(
        operations, {}, payload, descriptor);
    ASSERT_EQ(result.error, ContextualDynamicRangeEncodeError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x20}, std::byte{0x7F},
        std::byte{0xFF}, std::byte{0xBF}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        expected, std::span<const std::byte>{payload}.first(expected.size())));
    EXPECT_EQ(payload.back(), std::byte{0xCC});
}

TEST(ContextualDynamicRangeEncoder, EncodesDocumentedBypassVector) {
    constexpr auto operations = bypass_operations();
    ContextualDynamicRangeDescriptor descriptor{};
    std::array<std::byte, 6> payload{};
    const auto result = encode_contextual_dynamic_range_operations(
        operations, {}, payload, descriptor);
    ASSERT_EQ(result.error, ContextualDynamicRangeEncodeError::none);
    EXPECT_EQ(result.operation_count, 5U);
    EXPECT_EQ(result.decision_count, 6U);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0xA4}, std::byte{0x3C},
        std::byte{0x3C}, std::byte{0x38}, std::byte{0x00}};
    EXPECT_EQ(payload, expected);
}

TEST(ContextualDynamicRangeEncoder, DecoderRecoversEveryOperationValue) {
    constexpr auto operations = bypass_operations();
    ContextualDynamicRangeDescriptor descriptor{};
    std::array<std::byte, 6> payload{};
    ASSERT_EQ(encode_contextual_dynamic_range_operations(
                  operations, {}, payload, descriptor).error,
              ContextualDynamicRangeEncodeError::none);

    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value = UINT32_C(0xCCCCCCCC);
        const auto decoded = operation.kind == ModeledOperationKind::symbol
            ? decoder.decode_symbol(operation.context_id,
                                    operation.alphabet_size, value)
            : decoder.decode_bypass(operation.bit_count, value);
        ASSERT_EQ(decoded.error, ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    const auto finished = decoder.finish(
        static_cast<std::uint32_t>(operations.size()),
        descriptor.decision_count);
    EXPECT_EQ(finished.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(finished.payload_consumed, payload.size());
}

TEST(ContextualDynamicRangeEncoder, EncodesExtendedAlphabetAndBypassWidth) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 23, 21, 20, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0,
                         UINT32_C(0xABCDE), 20},
    };
    constexpr auto variant = LzssFieldContextVariant::field_context_1m;
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan = plan_contextual_dynamic_range_operations(
        operations, {}, descriptor, variant);
    ASSERT_EQ(plan.error, ContextualDynamicRangeEncodeError::none);
    EXPECT_EQ(plan.decision_count, 21U);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_contextual_dynamic_range_operations(
                  operations, {}, payload, descriptor, variant).error,
              ContextualDynamicRangeEncodeError::none);

    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, variant).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(23, 21, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 20U);
    ASSERT_EQ(decoder.decode_bypass(20, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, UINT32_C(0xABCDE));
    EXPECT_EQ(decoder.finish(2, 21).error,
              ContextualDynamicRangeDecodeError::none);

    descriptor = {};
    EXPECT_EQ(plan_contextual_dynamic_range_operations(
                  operations, {}, descriptor).error,
              ContextualDynamicRangeEncodeError::invalid_alphabet);
}

TEST(ContextualDynamicRangeEncoder, RescalesInLockstepWithDecoder) {
    std::vector<ModeledOperation> operations;
    operations.reserve(32770);
    for (std::uint32_t index = 0; index < 32770; ++index) {
        operations.push_back({ModeledOperationKind::symbol, 0, 2,
                              index & 1U, 0});
    }
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan = plan_contextual_dynamic_range_operations(
        operations, {}, descriptor);
    ASSERT_EQ(plan.error, ContextualDynamicRangeEncodeError::none);
    ASSERT_EQ(plan.decision_count, operations.size());
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_contextual_dynamic_range_operations(
                  operations, {}, payload, descriptor).error,
              ContextualDynamicRangeEncodeError::none);

    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{};
        ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
                  ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    EXPECT_EQ(decoder.finish(
                  static_cast<std::uint32_t>(operations.size()),
                  descriptor.decision_count).error,
              ContextualDynamicRangeDecodeError::none);
}

TEST(ContextualDynamicRangeEncoder, RejectsMalformedOperationsAtStableIndex) {
    struct Case {
        ModeledOperation operation;
        ContextualDynamicRangeEncodeError error;
    };
    constexpr std::array cases{
        Case{ModeledOperation{static_cast<ModeledOperationKind>(2), 0, 2, 0, 0},
             ContextualDynamicRangeEncodeError::invalid_operation_kind},
        Case{ModeledOperation{ModeledOperationKind::symbol, 31, 2, 0, 0},
             ContextualDynamicRangeEncodeError::invalid_context},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 3, 0, 0},
             ContextualDynamicRangeEncodeError::invalid_alphabet},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 2, 0},
             ContextualDynamicRangeEncodeError::invalid_symbol},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 1},
             ContextualDynamicRangeEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 1, 0, 0, 1},
             ContextualDynamicRangeEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 0},
             ContextualDynamicRangeEncodeError::invalid_bypass_width},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 4, 2},
             ContextualDynamicRangeEncodeError::nonzero_unused_field},
    };
    for (const auto& test : cases) {
        constexpr auto prefix = literal_operations();
        std::array operations{prefix[0], test.operation};
        ContextualDynamicRangeDescriptor descriptor{123, 456, 789};
        const auto result = plan_contextual_dynamic_range_operations(
            operations, {}, descriptor);
        EXPECT_EQ(result.error, test.error);
        EXPECT_EQ(result.operation_index, 1U);
        EXPECT_EQ(result.operation_count, 1U);
        EXPECT_EQ(descriptor.decision_count, 123U);
        EXPECT_EQ(descriptor.payload_size, 456U);
        EXPECT_EQ(descriptor.context_count, 789U);
    }
}

TEST(ContextualDynamicRangeEncoder, PrewriteFailuresPreserveAllOutput) {
    constexpr auto operations = literal_operations();
    ContextualDynamicRangeDescriptor descriptor{123, 456, 789};
    std::array<std::byte, 6> payload{};
    payload.fill(std::byte{0xCC});
    auto result = encode_contextual_dynamic_range_operations(
        operations, {}, std::span<std::byte>{payload}.first(5), descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<ModeledOperation, 1> storage{operations[0]};
    auto bytes = std::as_writable_bytes(std::span{storage});
    const auto before = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]};
    result = encode_contextual_dynamic_range_operations(
        storage, {}, bytes.first(6), descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
    EXPECT_EQ(descriptor.decision_count, 123U);
}

TEST(ContextualDynamicRangeEncoder, EnforcesEmptyAndLocalLimits) {
    ContextualDynamicRangeDescriptor descriptor{123, 456, 789};
    auto result = plan_contextual_dynamic_range_operations(
        {}, {}, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::empty_operations);

    constexpr auto operations = literal_operations();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries - 1;
    result = plan_contextual_dynamic_range_operations(
        operations, limits, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::limit_exceeded);

    limits = {};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v2 - 1;
    result = plan_contextual_dynamic_range_operations(
        operations, limits, descriptor,
        LzssFieldContextVariant::field_context_1m);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::limit_exceeded);

    limits = {};
    limits.max_range_model_total =
        contextual_dynamic_range_model_total_limit - 1;
    result = plan_contextual_dynamic_range_operations(
        operations, limits, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 5;
    result = plan_contextual_dynamic_range_operations(
        operations, limits, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::limit_exceeded);

    limits = {};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = sizeof(ModeledOperation) - 1;
    result = plan_contextual_dynamic_range_operations(
        operations, limits, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

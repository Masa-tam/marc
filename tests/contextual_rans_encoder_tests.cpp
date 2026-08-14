#include "entropy/contextual_rans_encoder.hpp"

#include "entropy/contextual_rans_decoder.hpp"

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

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] ContextualRansBeginResult begin_decoder(
    ContextualRansDecoder& decoder,
    const ContextualRansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const std::span<RansDecodeEntry> table_storage,
    const LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) {
    std::array<std::byte, contextual_rans_descriptor_capacity> bytes{};
    std::size_t written{};
    EXPECT_EQ(serialize_contextual_rans_descriptor(
                  descriptor, descriptor.decision_count,
                  descriptor.payload_size, {}, bytes, written, variant),
              ContextualRansFormatError::none);
    return decoder.begin(
        std::span<const std::byte>{bytes}.first(written),
        descriptor.decision_count, descriptor.payload_size, payload, {},
        table_storage, variant);
}

} // namespace

TEST(ContextualRansEncoder, PlansAndEncodesDocumentedLiteralVector) {
    constexpr auto operations = literal_operations();
    ContextualRansDescriptor descriptor{};
    auto result = plan_contextual_rans_operations(operations, {}, descriptor);
    ASSERT_EQ(result.error, ContextualRansEncodeError::none);
    EXPECT_EQ(result.operation_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 8U);
    EXPECT_EQ(descriptor.frequencies[0], 4096U);
    EXPECT_EQ(descriptor.frequencies[71], 4096U);
    EXPECT_EQ(std::ranges::count_if(
                  descriptor.frequencies,
                  [](const auto value) { return value != 0; }),
              2);

    std::array<std::byte, 9> payload{};
    payload.back() = std::byte{0xcc};
    descriptor = {};
    result = encode_contextual_rans_operations(
        operations, {}, payload, descriptor);
    ASSERT_EQ(result.error, ContextualRansEncodeError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        expected, std::span<const std::byte>{payload}.first(expected.size())));
    EXPECT_EQ(payload.back(), std::byte{0xcc});
}

TEST(ContextualRansEncoder, EncodesAndDecodesLsbFirstBypassVector) {
    constexpr auto operations = bypass_operations();
    ContextualRansDescriptor descriptor{};
    std::array<std::byte, 8> payload{};
    const auto result = encode_contextual_rans_operations(
        operations, {}, payload, descriptor);
    ASSERT_EQ(result.error, ContextualRansEncodeError::none);
    constexpr std::array expected{
        std::byte{0x00}, std::byte{0x10}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(payload, expected);
    EXPECT_EQ(descriptor.decision_count, 3U);
    EXPECT_EQ(descriptor.frequencies[1], 4096U);

    auto table_storage = tables();
    ContextualRansDecoder decoder;
    ASSERT_EQ(begin_decoder(decoder, descriptor, payload, table_storage)
                  .decode.error,
              ContextualRansDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 2U);
    EXPECT_EQ(decoder.finish(2, 3).error, ContextualRansDecodeError::none);
}

TEST(ContextualRansEncoder, ExtendedLayoutRoundTripsDistanceAndBypass) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 23, 21, 20, 0},
        ModeledOperation{
            ModeledOperationKind::bypass_bits, 0, 0, 0xabcde, 20},
    };
    constexpr auto extended = LzssFieldContextVariant::field_context_1m;

    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 0xa5a5;
    const auto before = descriptor;
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, {}, descriptor).error,
              ContextualRansEncodeError::invalid_alphabet);
    EXPECT_EQ(descriptor.decision_count, before.decision_count);
    std::array<std::byte, 64> rejected_payload{};
    rejected_payload.fill(std::byte{0xa5});
    EXPECT_EQ(encode_contextual_rans_operations(
                  operations, {}, rejected_payload, descriptor).error,
              ContextualRansEncodeError::invalid_alphabet);
    EXPECT_TRUE(std::ranges::all_of(
        rejected_payload,
        [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(descriptor.decision_count, before.decision_count);
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, {}, descriptor,
                  static_cast<LzssFieldContextVariant>(99)).error,
              ContextualRansEncodeError::unsupported_context_variant);
    EXPECT_EQ(descriptor.decision_count, before.decision_count);

    const auto plan = plan_contextual_rans_operations(
        operations, {}, descriptor, extended);
    ASSERT_EQ(plan.error, ContextualRansEncodeError::none);
    EXPECT_EQ(plan.decision_count, 21U);
    EXPECT_EQ(descriptor.frequency_entry_count,
              lzss_field_context_frequency_entries_v2);
    const auto offset = lzss_field_context_offsets_v2[23];
    EXPECT_EQ(descriptor.frequencies[offset + 20], 4096U);

    std::vector<std::byte> payload(plan.payload_size, std::byte{0xa5});
    ASSERT_EQ(encode_contextual_rans_operations(
                  operations, {}, payload, descriptor, extended).error,
              ContextualRansEncodeError::none);
    auto table_storage = tables();
    ContextualRansDecoder decoder;
    ASSERT_EQ(begin_decoder(
                  decoder, descriptor, payload, table_storage, extended)
                  .decode.error,
              ContextualRansDecodeError::none);
    std::uint32_t value{0xccccccccU};
    ASSERT_EQ(decoder.decode_symbol(23, 21, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 20U);
    value = 0xccccccccU;
    ASSERT_EQ(decoder.decode_bypass(20, value).error,
              ContextualRansDecodeError::none);
    EXPECT_EQ(value, 0xabcdeU);
    EXPECT_EQ(decoder.finish(2, 21).error, ContextualRansDecodeError::none);

    ContextualRansModelBuilder builder{extended};
    EXPECT_EQ(builder.add_bypass(16, 0), ContextualRansEncodeError::none);
    EXPECT_EQ(builder.add_bypass(17, 0), ContextualRansEncodeError::none);
    EXPECT_EQ(builder.add_bypass(20, 0), ContextualRansEncodeError::none);
    EXPECT_EQ(builder.add_bypass(21, 0),
              ContextualRansEncodeError::invalid_bypass_width);
    ContextualRansModelBuilder frozen_builder;
    EXPECT_EQ(frozen_builder.add_bypass(16, 0),
              ContextualRansEncodeError::none);
    EXPECT_EQ(frozen_builder.add_bypass(17, 0),
              ContextualRansEncodeError::invalid_bypass_width);
}

TEST(ContextualRansEncoder, NormalizesAndRoundTripsRenormalizedContext) {
    std::vector<ModeledOperation> operations;
    operations.reserve(512);
    for (std::uint32_t index = 0; index < 512; ++index) {
        operations.push_back(
            {ModeledOperationKind::symbol, 0, 2, index & 1U, 0});
    }
    ContextualRansDescriptor descriptor{};
    const auto plan = plan_contextual_rans_operations(
        operations, {}, descriptor);
    ASSERT_EQ(plan.error, ContextualRansEncodeError::none);
    ASSERT_GT(plan.payload_size, 8U);
    EXPECT_EQ(descriptor.frequencies[0], 2048U);
    EXPECT_EQ(descriptor.frequencies[1], 2048U);
    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_contextual_rans_operations(
                  operations, {}, payload, descriptor).error,
              ContextualRansEncodeError::none);

    auto table_storage = tables();
    ContextualRansDecoder decoder;
    ASSERT_EQ(begin_decoder(decoder, descriptor, payload, table_storage)
                  .decode.error,
              ContextualRansDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{};
        ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
                  ContextualRansDecodeError::none);
        EXPECT_EQ(value, operation.value);
    }
    EXPECT_EQ(decoder.finish(
                  static_cast<std::uint32_t>(operations.size()),
                  static_cast<std::uint32_t>(operations.size())).error,
              ContextualRansDecodeError::none);
}

TEST(ContextualRansEncoder, UsesDeterministicNumericNormalizationTies) {
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 2, 0},
    };
    ContextualRansDescriptor descriptor{};
    ASSERT_EQ(plan_contextual_rans_operations(
                  operations, {}, descriptor).error,
              ContextualRansEncodeError::none);
    const auto offset = lzss_field_context_offsets[3];
    EXPECT_EQ(descriptor.frequencies[offset], 1366U);
    EXPECT_EQ(descriptor.frequencies[offset + 1], 1365U);
    EXPECT_EQ(descriptor.frequencies[offset + 2], 1365U);
}

TEST(ContextualRansEncoder, RejectsMalformedOperationsAtStableIndex) {
    struct Case {
        ModeledOperation operation;
        ContextualRansEncodeError error;
    };
    constexpr std::array cases{
        Case{ModeledOperation{static_cast<ModeledOperationKind>(2), 0, 2, 0, 0},
             ContextualRansEncodeError::invalid_operation_kind},
        Case{ModeledOperation{ModeledOperationKind::symbol, 31, 2, 0, 0},
             ContextualRansEncodeError::invalid_context},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 3, 0, 0},
             ContextualRansEncodeError::invalid_alphabet},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 2, 0},
             ContextualRansEncodeError::invalid_symbol},
        Case{ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 1},
             ContextualRansEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 1, 0, 0, 1},
             ContextualRansEncodeError::nonzero_unused_field},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 0},
             ContextualRansEncodeError::invalid_bypass_width},
        Case{ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 4, 2},
             ContextualRansEncodeError::nonzero_unused_field},
    };
    for (const auto& test : cases) {
        constexpr auto prefix = literal_operations();
        std::array operations{prefix[0], test.operation};
        ContextualRansDescriptor descriptor{};
        descriptor.decision_count = 123;
        const auto result = plan_contextual_rans_operations(
            operations, {}, descriptor);
        EXPECT_EQ(result.error, test.error);
        EXPECT_EQ(result.operation_index, 1U);
        EXPECT_EQ(result.operation_count, 1U);
        EXPECT_EQ(descriptor.decision_count, 123U);
    }
}

TEST(ContextualRansEncoder, PrewriteFailuresPreservePayloadAndDescriptor) {
    constexpr auto operations = literal_operations();
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 123;
    std::array<std::byte, 8> payload{};
    payload.fill(std::byte{0xcc});
    auto result = encode_contextual_rans_operations(
        operations, {}, std::span<std::byte>{payload}.first(7), descriptor);
    EXPECT_EQ(result.error,
              ContextualRansEncodeError::payload_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(payload, [](const auto value) {
        return value == std::byte{0xcc};
    }));
    EXPECT_EQ(descriptor.decision_count, 123U);

    std::array<ModeledOperation, 1> storage{operations[0]};
    auto bytes = std::as_writable_bytes(std::span{storage});
    const auto before = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7]};
    result = encode_contextual_rans_operations(
        storage, {}, bytes.first(8), descriptor);
    EXPECT_EQ(result.error, ContextualRansEncodeError::overlapping_buffers);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
    EXPECT_EQ(descriptor.decision_count, 123U);
}

TEST(ContextualRansEncoder, EnforcesEmptyAndLocalLimits) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 123;
    EXPECT_EQ(plan_contextual_rans_operations({}, {}, descriptor).error,
              ContextualRansEncodeError::empty_operations);
    constexpr auto operations = literal_operations();

    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        contextual_rans_decode_table_entries - 1;
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, limits, descriptor).error,
              ContextualRansEncodeError::limit_exceeded);

    limits = {};
    limits.max_block_size = 1;
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, limits, descriptor).error,
              ContextualRansEncodeError::limit_exceeded);

    limits = {};
    limits.max_compressed_payload_size = 7;
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, limits, descriptor).error,
              ContextualRansEncodeError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = sizeof(ModeledOperation) - 1;
    EXPECT_EQ(plan_contextual_rans_operations(
                  operations, limits, descriptor).error,
              ContextualRansEncodeError::limit_exceeded);
    EXPECT_EQ(descriptor.decision_count, 123U);
}

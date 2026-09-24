#include "entropy/lzss_short_match_range_encoder.hpp"

#include "context/lzss_short_match_operations.hpp"
#include "entropy/lzss_short_match_range_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::entropy::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

constexpr marc::dictionary::internal::LzssParameters parameters{
    65536, 3, 258, 0};

constexpr std::array hand_tokens{
    LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
    LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};

constexpr std::array hand_payload{
    std::byte{0x00}, std::byte{0x30}, std::byte{0xbf}, std::byte{0xff},
    std::byte{0x9e}, std::byte{0x80}, std::byte{0x00}};

[[nodiscard]] std::array<ModeledOperation, 5> hand_operations() {
    std::array<ModeledOperation, 5> operations{};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        2, 4, 0};
    const auto result = marc::context::internal::model_lzss_short_match_tokens(
        hand_tokens, parameters, context, marc::core::DecoderLimits{},
        operations);
    EXPECT_EQ(result.error,
              marc::context::internal::LzssFieldContextError::none);
    return operations;
}

} // namespace

TEST(LzssShortMatchRangeEncoder, ReproducesIndependentHandPayload) {
    const auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    ContextualDynamicRangeDescriptor planned{};
    const auto plan = plan_lzss_short_match_range_operations(
        operations, limits, planned);
    ASSERT_EQ(plan.error, ContextualDynamicRangeEncodeError::none);
    EXPECT_EQ(plan.operation_count, 5U);
    EXPECT_EQ(plan.decision_count, 5U);
    EXPECT_EQ(plan.payload_size, hand_payload.size());
    EXPECT_EQ(planned.context_count, 32U);
    EXPECT_EQ(planned.decision_count, 5U);
    EXPECT_EQ(planned.payload_size, hand_payload.size());

    std::array<std::byte, 7> payload{};
    ContextualDynamicRangeDescriptor encoded_descriptor{};
    const auto encoded = encode_lzss_short_match_range_operations(
        operations, limits, payload, encoded_descriptor);
    ASSERT_EQ(encoded.error, ContextualDynamicRangeEncodeError::none);
    EXPECT_EQ(payload, hand_payload);
    EXPECT_EQ(encoded_descriptor.context_count, planned.context_count);
    EXPECT_EQ(encoded_descriptor.decision_count, planned.decision_count);
    EXPECT_EQ(encoded_descriptor.payload_size, planned.payload_size);
}

TEST(LzssShortMatchRangeEncoder, RoundTripsExtendedClassesAndBypass) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 4},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 4, 258}};
    std::array<ModeledOperation, 11> operations{};
    const auto limits = marc::core::DecoderLimits{};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        3, 263, 0};
    const auto modeled = marc::context::internal::model_lzss_short_match_tokens(
        tokens, parameters, context, limits, operations);
    ASSERT_EQ(modeled.error,
              marc::context::internal::LzssFieldContextError::none);

    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan = plan_lzss_short_match_range_operations(
        operations, limits, descriptor);
    ASSERT_EQ(plan.error, ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    const auto encoded = encode_lzss_short_match_range_operations(
        operations, limits, payload, descriptor);
    ASSERT_EQ(encoded.error, ContextualDynamicRangeEncodeError::none);
    LzssShortMatchRangeDecoder decoder{};
    ASSERT_EQ(decoder.begin(descriptor, payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    for (const auto& operation : operations) {
        std::uint32_t value{};
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
}

TEST(LzssShortMatchRangeEncoder, RescalesIdenticallyToPrivateDecoder) {
    constexpr std::size_t count = 32770;
    std::vector<ModeledOperation> operations(count);
    for (std::size_t index = 0; index < count; ++index) {
        operations[index] = {ModeledOperationKind::symbol, 0, 2,
                             static_cast<std::uint32_t>(index & 1U), 0};
    }
    const auto limits = marc::core::DecoderLimits{};
    ContextualDynamicRangeDescriptor descriptor{};
    const auto plan = plan_lzss_short_match_range_operations(
        operations, limits, descriptor);
    ASSERT_EQ(plan.error, ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(plan.payload_size);
    const auto encoded = encode_lzss_short_match_range_operations(
        operations, limits, payload, descriptor);
    ASSERT_EQ(encoded.error, ContextualDynamicRangeEncodeError::none);
    LzssShortMatchRangeDecoder decoder{};
    ASSERT_EQ(decoder.begin(descriptor, payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    for (std::size_t index = 0; index < count; ++index) {
        std::uint32_t value{};
        const auto decoded = decoder.decode_symbol(0, 2, value);
        ASSERT_EQ(decoded.error, ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(value, index & 1U);
    }
    EXPECT_EQ(decoder.finish(static_cast<std::uint32_t>(count),
                             descriptor.decision_count).error,
              ContextualDynamicRangeDecodeError::none);
}

TEST(LzssShortMatchRangeEncoder, RejectsInvalidOperationWithoutPublication) {
    auto operations = hand_operations();
    operations[4].context_id = 32;
    const auto limits = marc::core::DecoderLimits{};
    ContextualDynamicRangeDescriptor descriptor{99, 99, 99};
    const auto plan = plan_lzss_short_match_range_operations(
        operations, limits, descriptor);
    EXPECT_EQ(plan.error, ContextualDynamicRangeEncodeError::invalid_context);
    EXPECT_EQ(plan.operation_index, 4U);
    EXPECT_EQ(descriptor.decision_count, 99U);
    std::array<std::byte, 20> payload{};
    payload.fill(std::byte{0xcc});
    const auto encoded = encode_lzss_short_match_range_operations(
        operations, limits, payload, descriptor);
    EXPECT_EQ(encoded.error, ContextualDynamicRangeEncodeError::invalid_context);
    for (const auto byte : payload) EXPECT_EQ(byte, std::byte{0xcc});
    EXPECT_EQ(descriptor.context_count, 99U);
}

TEST(LzssShortMatchRangeEncoder, RejectsSmallOrOverlappingOutput) {
    auto operations = hand_operations();
    const auto limits = marc::core::DecoderLimits{};
    ContextualDynamicRangeDescriptor descriptor{99, 99, 99};
    std::array<std::byte, 6> too_small{};
    too_small.fill(std::byte{0xcc});
    auto result = encode_lzss_short_match_range_operations(
        operations, limits, too_small, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::payload_output_too_small);
    for (const auto byte : too_small) EXPECT_EQ(byte, std::byte{0xcc});
    EXPECT_EQ(descriptor.context_count, 99U);

    const auto alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(operations.data()),
        sizeof(operations)};
    result = encode_lzss_short_match_range_operations(
        operations, limits, alias, descriptor);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeEncodeError::overlapping_buffers);
    EXPECT_EQ(descriptor.context_count, 99U);
}

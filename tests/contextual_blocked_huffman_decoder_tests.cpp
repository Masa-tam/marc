#include "entropy/contextual_blocked_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace marc::entropy::internal;
using marc::context::internal::LzssFieldContextVariant;

[[nodiscard]] ContextualBlockedHuffmanDescriptor literal_a_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.field_active_mask = 0x03;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol = 0;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor mixed_descriptor() {
    auto descriptor = literal_a_descriptor();
    descriptor.decision_count = 7;
    descriptor.payload_size = 1;
    descriptor.final_valid_bits = 4;
    descriptor.field_active_mask = 0x0F;
    descriptor.field_models[0].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    descriptor.field_models[0].lengths[0] = 1;
    descriptor.field_models[0].lengths[1] = 1;
    descriptor.field_models[2].active = true;
    descriptor.field_models[2].single_symbol = 1;
    descriptor.field_models[3].active = true;
    descriptor.field_models[3].single_symbol = 1;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor extended_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 24;
    descriptor.payload_size = 3;
    descriptor.final_valid_bits = 4;
    descriptor.field_active_mask = 0x0f;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol = 1;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    descriptor.field_models[2].active = true;
    descriptor.field_models[2].single_symbol = 1;
    descriptor.field_models[3].active = true;
    descriptor.field_models[3].single_symbol = 20;
    return descriptor;
}

} // namespace

TEST(ContextualBlockedHuffmanDecoder, DecodesDocumentedOneLiteralVector) {
    ContextualBlockedHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);

    std::uint32_t value{0xCCCCCCCCU};
    auto result = decoder.decode_symbol(0, 2, value);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(result.bits_consumed, 0U);
    result = decoder.decode_symbol(3, 256, value);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.bits_consumed, 0U);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualBlockedHuffmanDecodeError::none);
}

TEST(ContextualBlockedHuffmanDecoder, DecodesMixedCodesAndBypassLsbFirst) {
    constexpr std::array payload{std::byte{0x06}};
    std::array<HuffmanDecodeTable, 1> tables{};
    ContextualBlockedHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(mixed_descriptor(), payload, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::none);

    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 65U);
    ASSERT_EQ(decoder.decode_symbol(1, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_symbol(21, 8, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_bypass(1, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_symbol(24, 17, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 1U);
    const auto result = decoder.decode_bypass(1, value);
    ASSERT_EQ(result.error, ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(result.event_count, 7U);
    EXPECT_EQ(result.decision_count, 7U);
    EXPECT_EQ(result.bits_consumed, 4U);
    EXPECT_EQ(decoder.finish(7, 7).error,
              ContextualBlockedHuffmanDecodeError::none);
}

TEST(ContextualBlockedHuffmanDecoder, SelectsOverrideBeforePooledFieldModel) {
    auto descriptor = literal_a_descriptor();
    descriptor.field_models[0].single_symbol = 1;
    descriptor.override_mask = 1;
    descriptor.context_models[0].active = true;
    descriptor.context_models[0].single_symbol = 0;
    ContextualBlockedHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);

    std::uint32_t value{0xCCCCCCCCU};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualBlockedHuffmanDecodeError::none);
}

TEST(ContextualBlockedHuffmanDecoder, EnforcesExtendedSelectedRequests) {
    constexpr std::array payload{
        std::byte{0xde}, std::byte{0xbc}, std::byte{0x0a}};
    constexpr auto extended = LzssFieldContextVariant::field_context_1m;
    std::array<HuffmanDecodeTable, 1> tables{};
    tables[0].node_count = 0xa5a5;

    ContextualBlockedHuffmanDecoder decoder;
    EXPECT_EQ(decoder.begin(
                  extended_descriptor(), payload, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::invalid_descriptor);
    EXPECT_EQ(tables[0].node_count, 0xa5a5U);

    const auto invalid = static_cast<LzssFieldContextVariant>(99);
    EXPECT_EQ(decoder.begin(
                  extended_descriptor(), payload, {}, tables, invalid).error,
              ContextualBlockedHuffmanDecodeError::invalid_descriptor);
    EXPECT_EQ(tables[0].node_count, 0xa5a5U);

    ASSERT_EQ(decoder.begin(
                  extended_descriptor(), payload, {}, tables, extended).error,
              ContextualBlockedHuffmanDecodeError::none);
    std::uint32_t value{0xccccccccU};
    EXPECT_EQ(decoder.decode_symbol(23, 17, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_alphabet);
    EXPECT_EQ(value, 0xccccccccU);

    ContextualBlockedHuffmanDecoder width_decoder;
    ASSERT_EQ(width_decoder.begin(
                  extended_descriptor(), payload, {}, tables, extended).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(width_decoder.decode_symbol(23, 21, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(value, 20U);
    value = 0xccccccccU;
    EXPECT_EQ(width_decoder.decode_bypass(21, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_bypass_width);
    EXPECT_EQ(value, 0xccccccccU);
}

TEST(ContextualBlockedHuffmanDecoder, RejectsBeginFailuresBeforeStarting) {
    std::array<HuffmanDecodeTable, 1> tables{};
    ContextualBlockedHuffmanDecoder decoder;
    constexpr std::array payload{std::byte{0x06}};

    EXPECT_EQ(decoder.begin(
                  mixed_descriptor(), {}, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::payload_size_mismatch);
    EXPECT_EQ(decoder.begin(
                  mixed_descriptor(), payload, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::table_output_too_small);

    auto padded = payload;
    padded[0] = std::byte{0x86};
    EXPECT_EQ(decoder.begin(
                  mixed_descriptor(), padded, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::nonzero_padding);

    auto* table_bytes = reinterpret_cast<std::byte*>(tables.data());
    table_bytes[0] = std::byte{0x06};
    const std::span<const std::byte> overlapping_payload{table_bytes, 1};
    EXPECT_EQ(decoder.begin(
                  mixed_descriptor(), overlapping_payload, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::overlapping_buffers);

    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::overlapping_buffers);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualBlockedHuffmanDecoder, EnforcesRequestsAndDecisionBudget) {
    std::uint32_t value{0xCCCCCCCCU};
    ContextualBlockedHuffmanDecoder context_decoder;
    ASSERT_EQ(context_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(context_decoder.decode_symbol(31, 2, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_context);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    ContextualBlockedHuffmanDecoder alphabet_decoder;
    ASSERT_EQ(alphabet_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(alphabet_decoder.decode_symbol(0, 256, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_alphabet);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    ContextualBlockedHuffmanDecoder inactive_decoder;
    ASSERT_EQ(inactive_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(inactive_decoder.decode_symbol(20, 8, value).error,
              ContextualBlockedHuffmanDecodeError::inactive_context);

    ContextualBlockedHuffmanDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(0, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_bypass_width);

    ContextualBlockedHuffmanDecoder budget_decoder;
    ASSERT_EQ(budget_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(budget_decoder.decode_bypass(3, value).error,
              ContextualBlockedHuffmanDecodeError::decision_count_exceeded);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualBlockedHuffmanDecoder, DetectsTruncationAndMutatedTable) {
    auto truncated = mixed_descriptor();
    truncated.payload_size = 0;
    truncated.final_valid_bits = 0;
    std::array<HuffmanDecodeTable, 1> tables{};
    ContextualBlockedHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(truncated, {}, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::none);
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::truncated_bits);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    constexpr std::array payload{std::byte{0x06}};
    ContextualBlockedHuffmanDecoder mutated_decoder;
    ASSERT_EQ(mutated_decoder.begin(
                  mixed_descriptor(), payload, {}, tables).error,
              ContextualBlockedHuffmanDecodeError::none);
    tables[0].fast.fill({});
    tables[0].nodes[0].child[0] = -1;
    EXPECT_EQ(mutated_decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::invalid_code);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualBlockedHuffmanDecoder, FinishRejectsCountsBitsAndUnusedOverride) {
    std::uint32_t value{};
    ContextualBlockedHuffmanDecoder count_decoder;
    ASSERT_EQ(count_decoder.begin(
                  literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(count_decoder.finish(1, 1).error,
              ContextualBlockedHuffmanDecodeError::count_mismatch);

    auto trailing = literal_a_descriptor();
    trailing.payload_size = 1;
    trailing.final_valid_bits = 1;
    constexpr std::array zero_bit{std::byte{0}};
    ContextualBlockedHuffmanDecoder trailing_decoder;
    ASSERT_EQ(trailing_decoder.begin(trailing, zero_bit, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(3, 256, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(2, 2).error,
              ContextualBlockedHuffmanDecodeError::trailing_bits);

    auto unused = literal_a_descriptor();
    unused.decision_count = 1;
    unused.override_mask = 1;
    unused.context_models[0].active = true;
    unused.context_models[0].single_symbol = 0;
    ContextualBlockedHuffmanDecoder unused_decoder;
    ASSERT_EQ(unused_decoder.begin(unused, {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(unused_decoder.decode_symbol(1, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(unused_decoder.finish(1, 1).error,
              ContextualBlockedHuffmanDecodeError::unused_override);
}

TEST(ContextualBlockedHuffmanDecoder, RequiresBeginAndEndsConsistently) {
    ContextualBlockedHuffmanDecoder decoder;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::not_started);
    ASSERT_EQ(decoder.begin(literal_a_descriptor(), {}, {}, {}).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualBlockedHuffmanDecodeError::none);
    ASSERT_EQ(decoder.finish(2, 2).error,
              ContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualBlockedHuffmanDecodeError::already_finished);
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualBlockedHuffmanDecodeError::already_finished);
}

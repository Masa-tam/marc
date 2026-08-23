#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::entropy::internal;

[[nodiscard]] constexpr auto literal_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0x20}, std::byte{0x7F},
        std::byte{0xFF}, std::byte{0xBF}, std::byte{0x00}};
}

[[nodiscard]] constexpr auto bypass_payload() {
    return std::array{
        std::byte{0x00}, std::byte{0xA4}, std::byte{0x3C},
        std::byte{0x3C}, std::byte{0x38}, std::byte{0x00}};
}

} // namespace

TEST(ContextualDynamicRangeDecoder, DecodesDocumentedOneLiteralVector) {
    constexpr auto payload = literal_payload();
    ContextualDynamicRangeDecoder decoder;
    auto result = decoder.begin({2, 6, 31}, payload, {});
    ASSERT_EQ(result.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(result.payload_consumed, 5U);

    std::uint32_t value{0xCCCCCCCCU};
    result = decoder.decode_symbol(0, 2, value);
    ASSERT_EQ(result.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(result.event_count, 1U);
    EXPECT_EQ(result.decision_count, 1U);

    value = 0xCCCCCCCCU;
    result = decoder.decode_symbol(3, 256, value);
    ASSERT_EQ(result.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(result.payload_consumed, payload.size());

    result = decoder.finish(2, 2);
    EXPECT_EQ(result.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
}

TEST(ContextualDynamicRangeDecoder, DecodesLsbFirstFixedBypassVector) {
    constexpr auto payload = bypass_payload();
    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin({6, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);

    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 1U);
    ASSERT_EQ(decoder.decode_symbol(20, 8, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 2U);
    ASSERT_EQ(decoder.decode_bypass(2, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 2U);
    ASSERT_EQ(decoder.decode_symbol(25, 17, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 1U);
    const auto bypass = decoder.decode_bypass(1, value);
    ASSERT_EQ(bypass.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(bypass.event_count, 5U);
    EXPECT_EQ(bypass.decision_count, 6U);
    EXPECT_EQ(decoder.finish(5, 6).error,
              ContextualDynamicRangeDecodeError::none);
}

TEST(ContextualDynamicRangeDecoder, SelectsExtendedAlphabetAndBypassWidth) {
    constexpr std::array<std::byte, 5> symbol_payload{};
    ContextualDynamicRangeDecoder symbol_decoder;
    ASSERT_EQ(symbol_decoder.begin(
                  {1, 5, 31}, symbol_payload, {},
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value{0xCCCCCCCCU};
    ASSERT_EQ(symbol_decoder.decode_symbol(23, 21, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(symbol_decoder.finish(1, 1).error,
              ContextualDynamicRangeDecodeError::none);

    constexpr std::array<std::byte, 7> bypass_payload{};
    ContextualDynamicRangeDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin(
                  {20, 7, 31}, bypass_payload, {},
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(bypass_decoder.decode_bypass(20, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(bypass_decoder.finish(1, 20).error,
              ContextualDynamicRangeDecodeError::none);

    ContextualDynamicRangeDecoder old_decoder;
    ASSERT_EQ(old_decoder.begin({20, 7, 31}, bypass_payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(old_decoder.decode_bypass(20, value).error,
              ContextualDynamicRangeDecodeError::invalid_bypass_width);
}

TEST(ContextualDynamicRangeDecoder, RejectsDescriptorAndPayloadMismatch) {
    constexpr auto payload = literal_payload();
    ContextualDynamicRangeDecoder decoder;
    EXPECT_EQ(decoder.begin({0, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);
    EXPECT_EQ(decoder.begin(
                  {2, 4, 31}, std::span{payload}.first<4>(), {}).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);
    EXPECT_EQ(decoder.begin({2, 6, 30}, payload, {}).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);
    EXPECT_EQ(decoder.begin({2, 5, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::payload_size_mismatch);

    auto malformed = payload;
    malformed[0] = std::byte{1};
    const auto result = decoder.begin({2, 6, 31}, malformed, {});
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::invalid_interval);
    EXPECT_EQ(result.payload_consumed, 1U);
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::invalid_interval);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualDynamicRangeDecoder, EnforcesFixedSchemaAndRequestWidths) {
    constexpr auto payload = literal_payload();
    std::uint32_t value{0xCCCCCCCCU};

    ContextualDynamicRangeDecoder context_decoder;
    ASSERT_EQ(context_decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    auto result = context_decoder.decode_symbol(31, 2, value);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::invalid_context);
    EXPECT_EQ(result.event_count, 0U);
    EXPECT_EQ(result.decision_count, 0U);
    EXPECT_EQ(result.payload_consumed, 5U);
    EXPECT_EQ(value, 0xCCCCCCCCU);
    EXPECT_EQ(context_decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::invalid_context);

    ContextualDynamicRangeDecoder alphabet_decoder;
    ASSERT_EQ(alphabet_decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    result = alphabet_decoder.decode_symbol(0, 256, value);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::invalid_alphabet);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    ContextualDynamicRangeDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    result = bypass_decoder.decode_bypass(0, value);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::invalid_bypass_width);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualDynamicRangeDecoder, PreservesValueOnTruncation) {
    constexpr auto complete = literal_payload();
    constexpr auto truncated = std::array{
        complete[0], complete[1], complete[2], complete[3], complete[4]};
    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin({2, 5, 31}, truncated, {}).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    value = 0xCCCCCCCCU;
    const auto result = decoder.decode_symbol(3, 256, value);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::truncated_payload);
    EXPECT_EQ(result.event_count, 1U);
    EXPECT_EQ(result.decision_count, 1U);
    EXPECT_EQ(result.payload_consumed, truncated.size());
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualDynamicRangeDecoder, EnforcesDecisionBudgetBeforeDecoding) {
    constexpr auto payload = literal_payload();
    ContextualDynamicRangeDecoder decoder;
    ASSERT_EQ(decoder.begin({1, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    value = 0xCCCCCCCCU;
    const auto result = decoder.decode_symbol(3, 256, value);
    EXPECT_EQ(result.error,
              ContextualDynamicRangeDecodeError::decision_count_exceeded);
    EXPECT_EQ(result.event_count, 1U);
    EXPECT_EQ(result.decision_count, 1U);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    ContextualDynamicRangeDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin({1, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(2, value).error,
              ContextualDynamicRangeDecodeError::decision_count_exceeded);
}

TEST(ContextualDynamicRangeDecoder, FinishRequiresExactCountsAndPayload) {
    constexpr auto payload = literal_payload();
    ContextualDynamicRangeDecoder count_decoder;
    ASSERT_EQ(count_decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value{};
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(3, 256, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(count_decoder.finish(3, 2).error,
              ContextualDynamicRangeDecodeError::count_mismatch);

    constexpr std::array trailing{
        payload[0], payload[1], payload[2], payload[3],
        payload[4], payload[5], std::byte{0}};
    ContextualDynamicRangeDecoder trailing_decoder;
    ASSERT_EQ(trailing_decoder.begin({2, 7, 31}, trailing, {}).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(3, 256, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(2, 2).error,
              ContextualDynamicRangeDecodeError::trailing_payload);
}

TEST(ContextualDynamicRangeDecoder, EnforcesTableAndModelLimits) {
    constexpr auto payload = literal_payload();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries - 1;
    ContextualDynamicRangeDecoder decoder;
    EXPECT_EQ(decoder.begin({2, 6, 31}, payload, limits).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);

    limits = {};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v2 - 1;
    EXPECT_EQ(decoder.begin(
                  {2, 6, 31}, payload, limits,
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_1m).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);

    limits = {};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v3 - 1;
    EXPECT_EQ(decoder.begin(
                  {2, 6, 31}, payload, limits,
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_4m).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);

    limits = {};
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v4 - 1;
    EXPECT_EQ(decoder.begin(
                  {2, 6, 31}, payload, limits,
                  marc::context::internal::LzssFieldContextVariant::
                      field_context_16m).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);

    limits = {};
    limits.max_range_model_total =
        contextual_dynamic_range_model_total_limit - 1;
    EXPECT_EQ(decoder.begin({2, 6, 31}, payload, limits).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);
}

TEST(ContextualDynamicRangeDecoder, RequiresBeginAndEndsConsistently) {
    ContextualDynamicRangeDecoder decoder;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::not_started);

    constexpr auto payload = literal_payload();
    ASSERT_EQ(decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(decoder.finish(2, 2).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualDynamicRangeDecodeError::already_finished);

    ASSERT_EQ(decoder.begin({2, 6, 31}, payload, {}).error,
              ContextualDynamicRangeDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualDynamicRangeDecodeError::none);
}

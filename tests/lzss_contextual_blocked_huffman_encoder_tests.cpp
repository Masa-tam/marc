#include "context/lzss_contextual_blocked_huffman_encoder.hpp"

#include "context/lzss_contextual_blocked_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::context::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualBlockedHuffmanDescriptor;
using marc::entropy::internal::HuffmanDecodeTable;

constexpr LzssTypedToken literal_a{
    LzssTypedTokenKind::literal, 'A', 0, 0};
constexpr LzssTypedToken match_1_5{
    LzssTypedTokenKind::match, 0, 1, 5};

[[nodiscard]] constexpr LzssTypedToken sentinel_token() {
    return {LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU};
}

[[nodiscard]] constexpr marc::dictionary::internal::
LzssTypedFrameValidationContext frame_context(
    const std::uint32_t tokens, const std::uint32_t raw) {
    return {tokens, raw, 0};
}

[[nodiscard]] std::vector<LzssTypedToken> four_mib_distance_tokens() {
    std::vector<LzssTypedToken> tokens;
    tokens.reserve(16259);
    tokens.push_back(literal_a);
    for (std::size_t index = 0; index < 16256; ++index) {
        tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    tokens.push_back({LzssTypedTokenKind::match, 0, 1, 255});
    tokens.push_back(
        {LzssTypedTokenKind::match, 0, UINT32_C(4194304), 5});
    return tokens;
}

[[nodiscard]] std::vector<LzssTypedToken> sixteen_mib_distance_tokens() {
    std::vector<LzssTypedToken> tokens;
    tokens.reserve(65030);
    tokens.push_back(literal_a);
    for (std::size_t index = 0; index < 65027; ++index) {
        tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    tokens.push_back({LzssTypedTokenKind::match, 0, 1, 249});
    tokens.push_back(
        {LzssTypedTokenKind::match, 0, UINT32_C(16777216), 5});
    return tokens;
}

} // namespace

TEST(LzssContextualBlockedHuffmanEncoder,
     PlansOneLiteralWithoutOperationWorkspace) {
    constexpr std::array tokens{literal_a};
    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto result = plan_lzss_contextual_blocked_huffman_tokens(
        tokens, {}, frame_context(1, 1), {}, descriptor);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.descriptor_size, 24U);
    EXPECT_EQ(result.payload_size, 0U);
    EXPECT_EQ(descriptor.field_models[0].single_symbol, 0U);
    EXPECT_EQ(descriptor.field_models[1].single_symbol, 'A');
}

TEST(LzssContextualBlockedHuffmanEncoder,
     EncodesLiteralMatchAndRoundTripsTypedTokens) {
    constexpr std::array tokens{literal_a, match_1_5};
    std::array payload{std::byte{0xcc}, std::byte{0xcc}};
    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto encoded = encode_lzss_contextual_blocked_huffman_tokens(
        tokens, {}, frame_context(2, 6), {}, payload, descriptor);
    ASSERT_EQ(encoded.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(encoded.event_count, 5U);
    EXPECT_EQ(encoded.decision_count, 5U);
    EXPECT_EQ(encoded.payload_size, 1U);
    EXPECT_EQ(descriptor.final_valid_bits, 2U);
    EXPECT_EQ(payload[0], std::byte{0x02});
    EXPECT_EQ(payload[1], std::byte{0xcc});

    std::array<HuffmanDecodeTable, 1> tables{};
    std::array<LzssTypedToken, 2> decoded{};
    const auto result = decode_lzss_contextual_blocked_huffman_tokens(
        descriptor, std::span<const std::byte>{payload}.first(1), {},
        {2, 5, 5, 6, 0}, {}, tables, decoded);
    ASSERT_EQ(result.error, LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_EQ(decoded[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(decoded[0].literal, 'A');
    EXPECT_EQ(decoded[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(decoded[1].distance, 1U);
    EXPECT_EQ(decoded[1].length, 5U);
}

TEST(LzssContextualBlockedHuffmanEncoder,
     MatchesModeledOperationBoundaryExactly) {
    constexpr std::array tokens{literal_a, match_1_5};
    constexpr std::array operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 'A', 0},
        ModeledOperation{ModeledOperationKind::symbol, 1, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 21, 8, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 23, 17, 0, 0}};
    std::array<std::byte, 1> token_payload{};
    std::array<std::byte, 1> operation_payload{};
    ContextualBlockedHuffmanDescriptor token_descriptor{};
    ContextualBlockedHuffmanDescriptor operation_descriptor{};
    const auto token_result = encode_lzss_contextual_blocked_huffman_tokens(
        tokens, {}, frame_context(2, 6), {}, token_payload, token_descriptor);
    const auto operation_result =
        marc::entropy::internal::encode_contextual_blocked_huffman_operations(
            operations, {}, operation_payload, operation_descriptor);
    ASSERT_EQ(token_result.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    ASSERT_EQ(operation_result.error,
              marc::entropy::internal::ContextualBlockedHuffmanEncodeError::
                  none);
    EXPECT_EQ(token_payload, operation_payload);
    std::array<std::byte, 64> token_bytes{};
    std::array<std::byte, 64> operation_bytes{};
    std::size_t token_size{};
    std::size_t operation_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  token_descriptor, 5, 1, {}, token_bytes, token_size),
              marc::entropy::internal::ContextualBlockedHuffmanFormatError::
                  none);
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  operation_descriptor, 5, 1, {}, operation_bytes,
                  operation_size),
              marc::entropy::internal::ContextualBlockedHuffmanFormatError::
                  none);
    EXPECT_EQ(token_size, operation_size);
    EXPECT_TRUE(std::equal(
        token_bytes.begin(), token_bytes.begin() + token_size,
        operation_bytes.begin()));
}

TEST(LzssContextualBlockedHuffmanEncoder,
     SelectedDistanceMatchesModeledBoundaryAndDecodesAtomically) {
    constexpr std::uint32_t distance = 131072;
    std::vector<LzssTypedToken> expected(distance + 1, literal_a);
    expected.back() = {LzssTypedTokenKind::match, 0, distance, 5};
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = 1048576;
    const auto limits = marc::core::DecoderLimits{};
    const auto context = frame_context(
        static_cast<std::uint32_t>(expected.size()), distance + 5);
    constexpr auto extended = LzssFieldContextVariant::field_context_1m;

    const auto operation_plan = plan_lzss_field_context_operations(
        expected, parameters, context, limits, extended);
    ASSERT_EQ(operation_plan.error, LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(operation_plan.operation_count);
    ASSERT_EQ(model_lzss_field_context_tokens(
                  expected, parameters, context, limits, operations,
                  extended).error,
              LzssFieldContextError::none);
    ASSERT_EQ(operations[operations.size() - 2].alphabet_size, 21U);
    ASSERT_EQ(operations[operations.size() - 2].value, 17U);
    ASSERT_EQ(operations.back().bit_count, 17U);

    ContextualBlockedHuffmanDescriptor reference_descriptor{};
    const auto reference_plan =
        marc::entropy::internal::plan_contextual_blocked_huffman_operations(
            operations, limits, reference_descriptor, extended);
    ASSERT_EQ(reference_plan.error,
              marc::entropy::internal::
                  ContextualBlockedHuffmanEncodeError::none);
    std::vector<std::byte> reference_payload(reference_plan.payload_size);
    ASSERT_EQ(
        marc::entropy::internal::encode_contextual_blocked_huffman_operations(
            operations, limits, reference_payload, reference_descriptor,
            extended).error,
        marc::entropy::internal::ContextualBlockedHuffmanEncodeError::none);

    ContextualBlockedHuffmanDescriptor direct_descriptor{};
    const auto direct_plan = plan_lzss_contextual_blocked_huffman_tokens(
        expected, parameters, context, limits, direct_descriptor, extended);
    ASSERT_EQ(direct_plan.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(direct_plan.event_count, operations.size());
    EXPECT_EQ(direct_plan.decision_count, reference_plan.decision_count);
    EXPECT_EQ(direct_plan.payload_size, reference_payload.size());

    std::vector<std::byte> direct_payload(direct_plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_tokens(
                  expected, parameters, context, limits, direct_payload,
                  direct_descriptor, extended).error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(direct_payload, reference_payload);

    std::array<std::byte,
               marc::entropy::internal::
                   contextual_blocked_huffman_descriptor_capacity>
        reference_bytes{};
    auto direct_bytes = reference_bytes;
    std::size_t reference_size{};
    std::size_t direct_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  reference_descriptor, reference_plan.decision_count,
                  static_cast<std::uint32_t>(reference_plan.payload_size),
                  limits, reference_bytes, reference_size, extended),
              marc::entropy::internal::
                  ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  direct_descriptor, direct_plan.decision_count,
                  static_cast<std::uint32_t>(direct_plan.payload_size), limits,
                  direct_bytes, direct_size, extended),
              marc::entropy::internal::
                  ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(direct_size, reference_size);
    EXPECT_TRUE(std::equal(
        reference_bytes.begin(), reference_bytes.begin() + reference_size,
        direct_bytes.begin()));

    std::array<HuffmanDecodeTable,
               marc::entropy::internal::
                   contextual_blocked_huffman_max_table_count>
        tables{};
    std::vector<LzssTypedToken> decoded(expected.size());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(direct_plan.event_count),
        direct_plan.decision_count, distance + 5, 0};
    const auto decoded_result =
        decode_lzss_contextual_blocked_huffman_tokens(
            direct_descriptor, direct_payload, parameters, decode_context,
            limits, tables, decoded, extended);
    ASSERT_EQ(decoded_result.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_TRUE(std::equal(
        expected.begin(), expected.end(), decoded.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xa5a5;
    auto rejected_payload = direct_payload;
    std::ranges::fill(rejected_payload, std::byte{0xa5});
    EXPECT_EQ(encode_lzss_contextual_blocked_huffman_tokens(
                  expected, parameters, context, limits, rejected_payload,
                  sentinel).error,
              LzssContextualBlockedHuffmanEncodeError::
                  token_validation_error);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);
    EXPECT_TRUE(std::ranges::all_of(
        rejected_payload,
        [](const auto value) { return value == std::byte{0xa5}; }));

    const auto invalid = static_cast<LzssFieldContextVariant>(99);
    EXPECT_EQ(plan_lzss_contextual_blocked_huffman_tokens(
                  expected, parameters, context, limits, sentinel,
                  invalid).entropy_error,
              marc::entropy::internal::
                  ContextualBlockedHuffmanEncodeError::
                      unsupported_context_variant);
    EXPECT_EQ(sentinel.decision_count, 0xa5a5U);

    for (auto& table : tables) table.node_count = 0xa5a5;
    std::ranges::fill(decoded, sentinel_token());
    const auto before = decoded;
    const auto legacy_parameters =
        marc::dictionary::internal::LzssParameters{};
    const auto crossed = decode_lzss_contextual_blocked_huffman_tokens(
        direct_descriptor, direct_payload, legacy_parameters, decode_context,
        limits, tables, decoded);
    EXPECT_EQ(crossed.error,
              LzssContextualBlockedHuffmanDecodeError::entropy_error);
    EXPECT_TRUE(std::equal(
        decoded.begin(), decoded.end(), before.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
    EXPECT_TRUE(std::ranges::all_of(
        tables, [](const auto& table) { return table.node_count == 0xa5a5; }));

    const auto unsupported = decode_lzss_contextual_blocked_huffman_tokens(
        direct_descriptor, direct_payload, parameters, decode_context, limits,
        tables, decoded, invalid);
    EXPECT_EQ(unsupported.error,
              LzssContextualBlockedHuffmanDecodeError::invalid_parameters);
    EXPECT_TRUE(std::equal(
        decoded.begin(), decoded.end(), before.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
    EXPECT_TRUE(std::ranges::all_of(
        tables, [](const auto& table) { return table.node_count == 0xa5a5; }));
}

TEST(LzssContextualBlockedHuffmanEncoder,
     FourMiBDistanceRoundTripsDirectTypedTokensAtomically) {
    const auto expected = four_mib_distance_tokens();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = UINT32_C(4194304);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = UINT64_C(4194309);
    const auto context = frame_context(
        static_cast<std::uint32_t>(expected.size()), UINT32_C(4194309));
    constexpr auto four_mib = LzssFieldContextVariant::field_context_4m;

    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto plan = plan_lzss_contextual_blocked_huffman_tokens(
        expected, parameters, context, limits, descriptor, four_mib);
    ASSERT_EQ(plan.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(plan.token_count, expected.size());
    EXPECT_EQ(plan.event_count, 65034U);
    EXPECT_EQ(plan.decision_count, 162597U);
    EXPECT_NE(descriptor.field_models[3].lengths[22], 0U);

    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_tokens(
                  expected, parameters, context, limits, payload, descriptor,
                  four_mib).error,
              LzssContextualBlockedHuffmanEncodeError::none);

    std::array<HuffmanDecodeTable,
               marc::entropy::internal::
                   contextual_blocked_huffman_max_table_count>
        tables{};
    std::vector<LzssTypedToken> decoded(expected.size(), sentinel_token());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(plan.event_count), plan.decision_count,
        UINT32_C(4194309), 0};
    const auto result = decode_lzss_contextual_blocked_huffman_tokens(
        descriptor, payload, parameters, decode_context, limits, tables,
        decoded, four_mib);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_TRUE(std::equal(
        expected.begin(), expected.end(), decoded.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    std::ranges::fill(decoded, sentinel_token());
    const auto before = decoded;
    for (auto& table : tables) table.node_count = 0xa5a5;
    const auto crossed = decode_lzss_contextual_blocked_huffman_tokens(
        descriptor, payload, parameters, decode_context, limits, tables,
        decoded, LzssFieldContextVariant::field_context_1m);
    EXPECT_NE(crossed.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_TRUE(std::equal(
        decoded.begin(), decoded.end(), before.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
    EXPECT_TRUE(std::ranges::all_of(
        tables, [](const auto& table) { return table.node_count == 0xa5a5; }));
}

TEST(LzssContextualBlockedHuffmanEncoder,
     SixteenMiBDistanceRoundTripsDirectTypedTokensAtomically) {
    const auto expected = sixteen_mib_distance_tokens();
    auto parameters = marc::dictionary::internal::LzssParameters{};
    parameters.window_size = UINT32_C(16777216);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT64_C(16777221);
    limits.max_block_size = UINT64_C(16777221);
    const auto context = frame_context(
        static_cast<std::uint32_t>(expected.size()), UINT32_C(16777221));
    constexpr auto sixteen_mib =
        LzssFieldContextVariant::field_context_16m;

    ContextualBlockedHuffmanDescriptor descriptor{};
    const auto plan = plan_lzss_contextual_blocked_huffman_tokens(
        expected, parameters, context, limits, descriptor, sixteen_mib);
    ASSERT_EQ(plan.error,
              LzssContextualBlockedHuffmanEncodeError::none);
    EXPECT_EQ(plan.token_count, expected.size());
    EXPECT_EQ(plan.event_count, 260118U);
    EXPECT_EQ(plan.decision_count, 650309U);
    EXPECT_NE(descriptor.field_models[3].lengths[24], 0U);

    std::vector<std::byte> payload(plan.payload_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_tokens(
                  expected, parameters, context, limits, payload, descriptor,
                  sixteen_mib).error,
              LzssContextualBlockedHuffmanEncodeError::none);

    std::array<HuffmanDecodeTable,
               marc::entropy::internal::
                   contextual_blocked_huffman_max_table_count>
        tables{};
    std::vector<LzssTypedToken> decoded(expected.size(), sentinel_token());
    const LzssFieldContextValidationContext decode_context{
        static_cast<std::uint32_t>(expected.size()),
        static_cast<std::uint32_t>(plan.event_count), plan.decision_count,
        UINT32_C(16777221), 0};
    const auto result = decode_lzss_contextual_blocked_huffman_tokens(
        descriptor, payload, parameters, decode_context, limits, tables,
        decoded, sixteen_mib);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_TRUE(std::equal(
        expected.begin(), expected.end(), decoded.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));

    std::ranges::fill(decoded, sentinel_token());
    const auto before = decoded;
    for (auto& table : tables) table.node_count = 0xa5a5;
    const auto crossed = decode_lzss_contextual_blocked_huffman_tokens(
        descriptor, payload, parameters, decode_context, limits, tables,
        decoded, LzssFieldContextVariant::field_context_4m);
    EXPECT_NE(crossed.error,
              LzssContextualBlockedHuffmanDecodeError::none);
    EXPECT_TRUE(std::equal(
        decoded.begin(), decoded.end(), before.begin(),
        [](const auto& left, const auto& right) {
            return left.kind == right.kind && left.literal == right.literal
                && left.distance == right.distance
                && left.length == right.length;
        }));
    EXPECT_TRUE(std::ranges::all_of(
        tables, [](const auto& table) { return table.node_count == 0xa5a5; }));
}

TEST(LzssContextualBlockedHuffmanEncoder, FailuresPreserveOutputs) {
    constexpr std::array tokens{literal_a, match_1_5};
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 0xCCCCCCCCU;
    auto result = encode_lzss_contextual_blocked_huffman_tokens(
        tokens, {}, frame_context(2, 6), {}, {}, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanEncodeError::
                  payload_output_too_small);
    EXPECT_EQ(descriptor.decision_count, 0xCCCCCCCCU);

    constexpr std::array invalid{
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 5}};
    result = plan_lzss_contextual_blocked_huffman_tokens(
        invalid, {}, frame_context(1, 5), {}, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanEncodeError::
                  token_validation_error);

    std::array token_storage{literal_a, match_1_5};
    auto bytes = std::as_writable_bytes(std::span{token_storage});
    result = encode_lzss_contextual_blocked_huffman_tokens(
        token_storage, {}, frame_context(2, 6), {}, bytes, descriptor);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanEncodeError::overlapping_buffers);
}

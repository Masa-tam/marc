#include "context/lzss_contextual_blocked_huffman_encoder.hpp"

#include "context/lzss_contextual_blocked_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

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

[[nodiscard]] constexpr marc::dictionary::internal::
LzssTypedFrameValidationContext frame_context(
    const std::uint32_t tokens, const std::uint32_t raw) {
    return {tokens, raw, 0};
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

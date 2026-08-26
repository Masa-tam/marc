#include "entropy/contextual_adaptive_huffman_format.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::entropy::internal::ContextualAdaptiveHuffmanDescriptor;
using marc::entropy::internal::ContextualAdaptiveHuffmanFormatError;

TEST(ContextualAdaptiveHuffmanFormat, SerializesAndParsesOneLiteralVector) {
    const ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    std::array<std::byte, 16> bytes{};
    ASSERT_EQ(
        marc::entropy::internal::
            serialize_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 2, {}, bytes),
        ContextualAdaptiveHuffmanFormatError::none);
    constexpr std::array expected{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(bytes, expected);

    ContextualAdaptiveHuffmanDescriptor parsed{};
    ASSERT_EQ(
        marc::entropy::internal::parse_contextual_adaptive_huffman_descriptor(
            bytes, 2, 2, {}, parsed),
        ContextualAdaptiveHuffmanFormatError::none);
    EXPECT_EQ(parsed.decision_count, 2U);
    EXPECT_EQ(parsed.payload_size, 2U);
    EXPECT_EQ(parsed.context_count, 31U);
    EXPECT_EQ(parsed.final_valid_bits, 1U);
    EXPECT_EQ(parsed.flags, 0U);
}

TEST(ContextualAdaptiveHuffmanFormat, RejectsFieldsAndContradictions) {
    ContextualAdaptiveHuffmanDescriptor descriptor{2, 2, 31, 1, 0};
    descriptor.decision_count = 0;
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 0, 2, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_decision_count);
    descriptor = {2, 0, 31, 1, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 0, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_payload_size);
    descriptor = {2, 2, 30, 1, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 2, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_context_count);
    descriptor = {2, 2, 31, 0, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 2, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_final_bits);
    descriptor.final_valid_bits = 9;
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 2, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_final_bits);
    descriptor = {2, 2, 31, 1, 1};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 2, 2, {}),
        ContextualAdaptiveHuffmanFormatError::unknown_flags);
    descriptor = {2, 2, 31, 1, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, 3, 2, {}),
        ContextualAdaptiveHuffmanFormatError::contradictory_size);
}

TEST(ContextualAdaptiveHuffmanFormat, RejectsReservedAndLimitsAtomically) {
    constexpr std::array valid{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    auto malformed = valid;
    malformed[15] = std::byte{1};
    ContextualAdaptiveHuffmanDescriptor parsed{9, 9, 31, 8, 0xa5};
    EXPECT_EQ(
        marc::entropy::internal::parse_contextual_adaptive_huffman_descriptor(
            malformed, 2, 2, {}, parsed),
        ContextualAdaptiveHuffmanFormatError::nonzero_reserved);
    EXPECT_EQ(parsed.decision_count, 9U);
    EXPECT_EQ(parsed.flags, 0xa5U);

    marc::core::DecoderLimits limits{};
    limits.max_compressed_payload_size = 1;
    EXPECT_EQ(
        marc::entropy::internal::parse_contextual_adaptive_huffman_descriptor(
            valid, 2, 2, limits, parsed),
        ContextualAdaptiveHuffmanFormatError::limit_exceeded);
    EXPECT_EQ(parsed.decision_count, 9U);

    auto output = valid;
    const auto before = output;
    const ContextualAdaptiveHuffmanDescriptor invalid{2, 2, 30, 1, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            serialize_contextual_adaptive_huffman_descriptor(
                invalid, 2, 2, {}, output),
        ContextualAdaptiveHuffmanFormatError::invalid_context_count);
    EXPECT_EQ(output, before);
}

TEST(ContextualAdaptiveHuffmanFormat, AcceptsTheExactDecisionCeiling) {
    static_assert(
        marc::entropy::internal::
            contextual_adaptive_huffman_max_decision_count
        == UINT32_C(117440512));
    const ContextualAdaptiveHuffmanDescriptor descriptor{
        marc::entropy::internal::
            contextual_adaptive_huffman_max_decision_count,
        1, 31, 8, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, descriptor.decision_count, 1, {}),
        ContextualAdaptiveHuffmanFormatError::none);
}

TEST(ContextualAdaptiveHuffmanFormat, RejectsAboveTheDecisionCeiling) {
    ContextualAdaptiveHuffmanDescriptor descriptor{
        marc::entropy::internal::
                contextual_adaptive_huffman_max_decision_count
            + 1U,
        1, 31, 8, 0};
    EXPECT_EQ(
        marc::entropy::internal::
            validate_contextual_adaptive_huffman_descriptor(
                descriptor, descriptor.decision_count, 1, {}),
        ContextualAdaptiveHuffmanFormatError::invalid_decision_count);
}

} // namespace

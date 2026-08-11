#include "entropy/contextual_adaptive_huffman_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::entropy::internal;

struct DecoderWorkspace {
    std::array<AdaptiveHuffmanNode,
               contextual_adaptive_huffman_node_entries>
        nodes{};
    std::array<std::uint16_t,
               contextual_adaptive_huffman_symbol_entries>
        symbols{};
};

[[nodiscard]] ContextualAdaptiveHuffmanDescriptor literal_a_descriptor() {
    return {2, 2, 31, 1, 0};
}

TEST(ContextualAdaptiveHuffmanDecoder, DecodesDocumentedOneLiteralVector) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(literal_a_descriptor(), payload, {},
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);

    std::uint32_t value{0xCCCCCCCCU};
    auto result = decoder.decode_symbol(0, 2, value);
    ASSERT_EQ(result.error, ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    EXPECT_EQ(result.bits_consumed, 1U);
    result = decoder.decode_symbol(3, 256, value);
    ASSERT_EQ(result.error, ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 65U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.bits_consumed, 9U);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualAdaptiveHuffmanDecodeError::none);
}

TEST(ContextualAdaptiveHuffmanDecoder, DecodesExistingNytAndBypassLsbFirst) {
    constexpr std::array payload{std::byte{0x5a}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{6, 1, 31, 7, 0};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, workspace.nodes,
                            workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);

    std::uint32_t value{};
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 0U);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 1U);
    const auto result = decoder.decode_bypass(3, value);
    ASSERT_EQ(result.error, ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(value, 5U);
    EXPECT_EQ(result.event_count, 4U);
    EXPECT_EQ(result.decision_count, 6U);
    EXPECT_EQ(result.bits_consumed, 7U);
    EXPECT_EQ(decoder.finish(4, 6).error,
              ContextualAdaptiveHuffmanDecodeError::none);
}

TEST(ContextualAdaptiveHuffmanDecoder, RejectsBeginFailuresAndOverlap) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    EXPECT_EQ(decoder.begin(literal_a_descriptor(),
                            std::span{payload}.first<1>(), {},
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::payload_size_mismatch);
    EXPECT_EQ(decoder.begin(
                  literal_a_descriptor(), payload, {},
                  std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
                  workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::node_workspace_too_small);
    EXPECT_EQ(decoder.begin(
                  literal_a_descriptor(), payload, {}, workspace.nodes,
                  std::span{workspace.symbols}.first(
                      workspace.symbols.size() - 1)).error,
              ContextualAdaptiveHuffmanDecodeError::symbol_workspace_too_small);

    auto padded = payload;
    padded[1] = std::byte{0x80};
    EXPECT_EQ(decoder.begin(literal_a_descriptor(), padded, {},
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::nonzero_padding);

    workspace.nodes[0] = {};
    const std::span<const std::byte> overlapping_payload{
        reinterpret_cast<const std::byte*>(workspace.nodes.data()), 2};
    EXPECT_EQ(decoder.begin(literal_a_descriptor(), overlapping_payload, {},
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::overlapping_buffers);

    auto overlapping_symbols = std::span<std::uint16_t>{
        reinterpret_cast<std::uint16_t*>(workspace.nodes.data()),
        contextual_adaptive_huffman_symbol_entries};
    EXPECT_EQ(decoder.begin(literal_a_descriptor(), payload, {},
                            workspace.nodes, overlapping_symbols).error,
              ContextualAdaptiveHuffmanDecodeError::overlapping_buffers);
}

TEST(ContextualAdaptiveHuffmanDecoder, FailureDoesNotPublishOrCommitBits) {
    constexpr std::array one_bit{std::byte{0x00}};
    const ContextualAdaptiveHuffmanDescriptor truncated{1, 1, 31, 1, 0};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(truncated, one_bit, {}, workspace.nodes,
                            workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    std::uint32_t value{0xCCCCCCCCU};
    const auto result = decoder.decode_symbol(3, 256, value);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanDecodeError::truncated_bits);
    EXPECT_EQ(result.bits_consumed, 0U);
    EXPECT_EQ(result.event_count, 0U);
    EXPECT_EQ(value, 0xCCCCCCCCU);
    EXPECT_EQ(decoder.decode_bypass(1, value).error,
              ContextualAdaptiveHuffmanDecodeError::truncated_bits);
}

TEST(ContextualAdaptiveHuffmanDecoder, RejectsUnusedNytValuesAtomically) {
    constexpr std::array payload{std::byte{0x1f}};
    const ContextualAdaptiveHuffmanDescriptor descriptor{1, 1, 31, 5, 0};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, payload, {}, workspace.nodes,
                            workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    std::uint32_t value{0xCCCCCCCCU};
    const auto result = decoder.decode_symbol(23, 17, value);
    EXPECT_EQ(result.error,
              ContextualAdaptiveHuffmanDecodeError::invalid_nyt_symbol);
    EXPECT_EQ(result.bits_consumed, 0U);
    EXPECT_EQ(value, 0xCCCCCCCCU);
}

TEST(ContextualAdaptiveHuffmanDecoder, EnforcesRequestsAndDecisionBudget) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    std::uint32_t value{0xCCCCCCCCU};

    DecoderWorkspace context_workspace{};
    ContextualAdaptiveHuffmanDecoder context_decoder;
    ASSERT_EQ(context_decoder.begin(literal_a_descriptor(), payload, {},
                                    context_workspace.nodes,
                                    context_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(context_decoder.decode_symbol(31, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_context);
    EXPECT_EQ(value, 0xCCCCCCCCU);

    DecoderWorkspace alphabet_workspace{};
    ContextualAdaptiveHuffmanDecoder alphabet_decoder;
    ASSERT_EQ(alphabet_decoder.begin(literal_a_descriptor(), payload, {},
                                     alphabet_workspace.nodes,
                                     alphabet_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(alphabet_decoder.decode_symbol(0, 256, value).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_alphabet);

    DecoderWorkspace bypass_workspace{};
    ContextualAdaptiveHuffmanDecoder bypass_decoder;
    ASSERT_EQ(bypass_decoder.begin(literal_a_descriptor(), payload, {},
                                   bypass_workspace.nodes,
                                   bypass_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(bypass_decoder.decode_bypass(0, value).error,
              ContextualAdaptiveHuffmanDecodeError::invalid_bypass_width);

    DecoderWorkspace budget_workspace{};
    ContextualAdaptiveHuffmanDecoder budget_decoder;
    ASSERT_EQ(budget_decoder.begin(literal_a_descriptor(), payload, {},
                                   budget_workspace.nodes,
                                   budget_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(budget_decoder.decode_bypass(3, value).error,
              ContextualAdaptiveHuffmanDecodeError::decision_count_exceeded);
}

TEST(ContextualAdaptiveHuffmanDecoder, EnforcesModelAndAggregateLimits) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    DecoderWorkspace workspace{};
    ContextualAdaptiveHuffmanDecoder decoder;
    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries =
        contextual_adaptive_huffman_node_entries
        + contextual_adaptive_huffman_symbol_entries - 1;
    EXPECT_EQ(decoder.begin(literal_a_descriptor(), payload, limits,
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes =
        contextual_adaptive_huffman_node_entries
            * sizeof(AdaptiveHuffmanNode)
        + contextual_adaptive_huffman_symbol_entries * sizeof(std::uint16_t)
        + payload.size() - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(decoder.begin(literal_a_descriptor(), payload, limits,
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::limit_exceeded);
}

TEST(ContextualAdaptiveHuffmanDecoder, FinishRejectsCountsAndTrailingBits) {
    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    std::uint32_t value{};
    DecoderWorkspace count_workspace{};
    ContextualAdaptiveHuffmanDecoder count_decoder;
    ASSERT_EQ(count_decoder.begin(literal_a_descriptor(), payload, {},
                                  count_workspace.nodes,
                                  count_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(count_decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(count_decoder.finish(1, 1).error,
              ContextualAdaptiveHuffmanDecodeError::count_mismatch);

    const ContextualAdaptiveHuffmanDescriptor trailing{2, 2, 31, 2, 0};
    DecoderWorkspace trailing_workspace{};
    ContextualAdaptiveHuffmanDecoder trailing_decoder;
    ASSERT_EQ(trailing_decoder.begin(trailing, payload, {},
                                     trailing_workspace.nodes,
                                     trailing_workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(trailing_decoder.decode_symbol(3, 256, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(trailing_decoder.finish(2, 2).error,
              ContextualAdaptiveHuffmanDecodeError::trailing_bits);
}

TEST(ContextualAdaptiveHuffmanDecoder, RequiresBeginAndEndsConsistently) {
    ContextualAdaptiveHuffmanDecoder decoder;
    std::uint32_t value{0xCCCCCCCCU};
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::not_started);

    constexpr std::array payload{std::byte{0x82}, std::byte{0x00}};
    DecoderWorkspace workspace{};
    ASSERT_EQ(decoder.begin(literal_a_descriptor(), payload, {},
                            workspace.nodes, workspace.symbols).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(decoder.decode_symbol(3, 256, value).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    ASSERT_EQ(decoder.finish(2, 2).error,
              ContextualAdaptiveHuffmanDecodeError::none);
    EXPECT_EQ(decoder.finish(2, 2).error,
              ContextualAdaptiveHuffmanDecodeError::already_finished);
    EXPECT_EQ(decoder.decode_symbol(0, 2, value).error,
              ContextualAdaptiveHuffmanDecodeError::already_finished);
}

} // namespace

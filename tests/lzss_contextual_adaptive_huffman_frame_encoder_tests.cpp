#include "frame/lzss_contextual_adaptive_huffman_frame_encoder.hpp"

#include "frame/lzss_contextual_adaptive_huffman_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;

struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_for(
    const std::uint64_t original_size) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> documented_literal_frame() {
    std::vector<std::byte> frame(82);
    const auto stream = stream_for(1);
    const LzssContextualAdaptiveHuffmanFrameHeader header{
        0, 0, 1, 1, 2, 2, 2, 16, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, frame.begin() + 64);
    frame[80] = std::byte{0x82};
    frame[81] = std::byte{0x00};
    return frame;
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     PlansAndEmitsDocumentedLiteralFrame) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xcc;
    Workspace workspace{};

    auto result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    EXPECT_EQ(result.serialized_size, 82U);
    EXPECT_EQ(result.descriptor_size, 16U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(result.required_node_entries,
              contextual_adaptive_huffman_node_entries);
    EXPECT_EQ(result.required_symbol_entries,
              contextual_adaptive_huffman_symbol_entries);

    std::vector<std::byte> output(result.serialized_size + 1,
                                  std::byte{0xcc});
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols,
        output);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        documented_literal_frame(),
        std::span<const std::byte>{output}.first(result.serialized_size)));
    EXPECT_EQ(output.back(), std::byte{0xcc});
    EXPECT_EQ(tokens[1].literal, 0xcc);
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     CompleteDecoderRecoversLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> encode_tokens{};
    Workspace encode_workspace{};
    std::array<std::byte, 82> frame{};
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_frame(
                  stream, {}, 0, 0, raw, encode_tokens,
                  encode_workspace.nodes, encode_workspace.symbols, frame)
                  .error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);

    Workspace decode_workspace{};
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, decode_workspace.nodes,
        decode_workspace.symbols, decode_tokens, decoded);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     RoundTripsMixedRawFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens_a{};
    Workspace workspace_a{};
    const auto plan = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens_a, workspace_a.nodes,
        workspace_a.symbols);
    ASSERT_EQ(plan.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    std::vector<std::byte> first(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_frame(
                  stream, {}, 0, 0, raw, tokens_a, workspace_a.nodes,
                  workspace_a.symbols, first)
                  .error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);

    std::array<LzssTypedToken, raw.size()> tokens_b{};
    Workspace workspace_b{};
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_frame(
                  stream, {}, 0, 0, raw, tokens_b, workspace_b.nodes,
                  workspace_b.symbols, second)
                  .error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    EXPECT_EQ(second, first);

    Workspace decoder_workspace{};
    std::array<LzssTypedToken, raw.size()> decode_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    const auto decoded_result =
        decode_lzss_contextual_adaptive_huffman_frame(
            first, {stream, {}, 0, 0}, decoder_workspace.nodes,
            decoder_workspace.symbols, decode_tokens, decoded);
    ASSERT_EQ(decoded_result.error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     HashChainMatchesExhaustiveAndRejectsWorkspaceFailuresAtomically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> exhaustive_tokens{};
    Workspace exhaustive_workspace{};
    const auto exhaustive_plan = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, exhaustive_tokens,
        exhaustive_workspace.nodes, exhaustive_workspace.symbols);
    ASSERT_EQ(exhaustive_plan.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    std::vector<std::byte> exhaustive(exhaustive_plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_frame(
                  stream, {}, 0, 0, raw, exhaustive_tokens,
                  exhaustive_workspace.nodes, exhaustive_workspace.symbols,
                  exhaustive).error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);

    const auto finder_requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(raw.size(), stream.dictionary, {});
    ASSERT_EQ(finder_requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    std::vector<std::max_align_t> finder_backing(
        (finder_requirements.workspace_size + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t));
    auto finder = std::as_writable_bytes(std::span{finder_backing}).first(
        finder_requirements.workspace_size);
    std::array<LzssTypedToken, raw.size()> hash_tokens{};
    Workspace hash_workspace{};
    marc::dictionary::internal::LzssMatchFinderStatistics statistics{};
    const auto hash_plan =
        plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
            stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
            hash_workspace.symbols, finder, &statistics);
    ASSERT_EQ(hash_plan.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    EXPECT_EQ(hash_plan.serialized_size, exhaustive_plan.serialized_size);
    EXPECT_EQ(hash_plan.descriptor_size, exhaustive_plan.descriptor_size);
    EXPECT_EQ(hash_plan.token_count, exhaustive_plan.token_count);
    EXPECT_EQ(hash_plan.event_count, exhaustive_plan.event_count);
    EXPECT_EQ(hash_plan.decision_count, exhaustive_plan.decision_count);
    EXPECT_EQ(hash_plan.payload_size, exhaustive_plan.payload_size);
    EXPECT_EQ(statistics.query_count, hash_plan.token_count);

    std::vector<std::byte> hash(hash_plan.serialized_size);
    statistics = {};
    ASSERT_EQ(encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
                  stream, {}, 0, 0, raw, hash_tokens,
                  hash_workspace.nodes, hash_workspace.symbols, finder, hash,
                  &statistics).error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    EXPECT_EQ(hash, exhaustive);
    EXPECT_EQ(statistics.query_count, hash_plan.token_count);

    Workspace decoder_workspace{};
    std::array<LzssTypedToken, raw.size()> decode_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    ASSERT_EQ(decode_lzss_contextual_adaptive_huffman_frame(
                  hash, {stream, {}, 0, 0}, decoder_workspace.nodes,
                  decoder_workspace.symbols, decode_tokens, decoded).error,
              LzssContextualAdaptiveHuffmanFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);

    std::ranges::fill(hash, std::byte{0xcc});
    auto failed = encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols, finder.first(finder.size() - 1), hash);
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  token_encode_error);
    EXPECT_EQ(failed.token_encode.match_finder_error,
              marc::dictionary::internal::LzssHashChainError::
                  workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(hash, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    auto raw_copy = raw;
    failed = plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw_copy, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols, std::span<std::byte>{raw_copy});
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  overlapping_workspaces);
    failed = plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols,
        std::as_writable_bytes(std::span{hash_tokens}));
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  overlapping_workspaces);
    failed = plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols,
        std::as_writable_bytes(std::span{hash_workspace.nodes}));
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  overlapping_workspaces);
    failed = plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols,
        std::as_writable_bytes(std::span{hash_workspace.symbols}));
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  overlapping_workspaces);
    std::vector<std::byte> shared(
        std::max(finder.size(), hash.size()), std::byte{0xcc});
    failed = encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, {}, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols,
        std::span<std::byte>{shared}.first(finder.size()),
        std::span<std::byte>{shared}.first(hash.size()));
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::
                  overlapping_workspaces);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = raw.size()
        + raw.size() * sizeof(LzssTypedToken)
        + contextual_adaptive_huffman_node_entries
            * sizeof(AdaptiveHuffmanNode)
        + contextual_adaptive_huffman_symbol_entries * sizeof(std::uint16_t)
        + finder.size() + hash_plan.serialized_size - 1;
    failed = plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
        stream, limits, 0, 0, raw, hash_tokens, hash_workspace.nodes,
        hash_workspace.symbols, finder);
    EXPECT_EQ(failed.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::workspace_limit);
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     CapacityFailuresPreserveSerializedOutput) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    Workspace workspace{};
    std::vector<std::byte> output(82, std::byte{0xcc});

    auto result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, {}, workspace.nodes, workspace.symbols,
        output);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                token_staging_too_small);
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens,
        std::span{workspace.nodes}.first(workspace.nodes.size() - 1),
        workspace.symbols, output);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                node_staging_too_small);
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes,
        std::span{workspace.symbols}.first(workspace.symbols.size() - 1),
        output);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                symbol_staging_too_small);
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols,
        std::span<std::byte>{output}.first(output.size() - 1));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     RejectsAliasesBeforeSerializedWrites) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    Workspace workspace{};
    std::array<std::byte, 82> output{};

    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    token_bytes[0] = std::byte{'A'};
    auto result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{token_bytes}.first(1), tokens,
        workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);

    auto node_bytes = std::as_writable_bytes(std::span{workspace.nodes});
    node_bytes[0] = std::byte{'A'};
    result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{node_bytes}.first(1), tokens,
        workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);

    auto symbol_bytes = std::as_writable_bytes(std::span{workspace.symbols});
    symbol_bytes[0] = std::byte{'A'};
    result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{symbol_bytes}.first(1), tokens,
        workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);

    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols,
        token_bytes);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols,
        node_bytes.first(82));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols,
        symbol_bytes.first(82));
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);

    output[0] = std::byte{'A'};
    result = encode_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{output}.first(1), tokens, workspace.nodes,
        workspace.symbols, output);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                overlapping_workspaces);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssContextualAdaptiveHuffmanFrameEncoder,
     RejectsStreamInputAndAggregateWorkspaceLimit) {
    constexpr std::array raw{std::byte{'A'}};
    auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    Workspace workspace{};

    stream.context_count = 30;
    auto result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::invalid_stream);

    stream = stream_for(2);
    result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                input_size_mismatch);

    stream = stream_for(raw.size());
    result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, {}, 0, 0, raw, tokens, workspace.nodes, workspace.symbols);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFrameEncodeError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        raw.size() + result.token_encode.token_storage_size
        + contextual_adaptive_huffman_node_entries
            * sizeof(AdaptiveHuffmanNode)
        + contextual_adaptive_huffman_symbol_entries * sizeof(std::uint16_t)
        + result.serialized_size - 1;
    result = plan_lzss_contextual_adaptive_huffman_frame(
        stream, limits, 0, 0, raw, tokens, workspace.nodes,
        workspace.symbols);
    EXPECT_EQ(result.error, LzssContextualAdaptiveHuffmanFrameEncodeError::
                                workspace_limit);
}

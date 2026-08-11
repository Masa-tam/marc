#include "frame/lzss_contextual_blocked_huffman_frame_encoder.hpp"

#include "frame/lzss_contextual_blocked_huffman_frame_decoder.hpp"

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
using marc::entropy::internal::HuffmanDecodeTable;

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader stream_for(
    const std::uint64_t original_size) {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> documented_literal_frame() {
    std::vector<std::byte> bytes(88);
    constexpr std::array header{
        std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x32},
        std::byte{0x40}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{24}, std::byte{0}, std::byte{0}, std::byte{0}};
    std::ranges::copy(header, bytes.begin());
    constexpr std::array descriptor{
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{15}, std::byte{3}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{65}, std::byte{0}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

} // namespace

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     PlansAndEmitsDocumentedLiteralFrame) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xcc;
    auto result = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    EXPECT_EQ(result.serialized_size, 88U);
    EXPECT_EQ(result.descriptor_size, 24U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 0U);

    std::vector<std::byte> output(result.serialized_size + 1, std::byte{0xcc});
    result = encode_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens, output);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        documented_literal_frame(),
        std::span<const std::byte>{output}.first(result.serialized_size)));
    EXPECT_EQ(output.back(), std::byte{0xcc});
    EXPECT_EQ(tokens[1].literal, 0xcc);
}

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     CompleteDecoderRecoversLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> encode_tokens{};
    std::array<std::byte, 88> frame{};
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_frame(
                  stream, {}, 0, 0, raw, encode_tokens, frame).error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_blocked_huffman_frame(
        frame, {stream, {}, 0, 0}, {}, decode_tokens, decoded);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     RoundTripsMixedRawFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens_a{};
    const auto plan = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens_a);
    ASSERT_EQ(plan.error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    std::vector<std::byte> first(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_frame(
                  stream, {}, 0, 0, raw, tokens_a, first).error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    std::array<LzssTypedToken, raw.size()> tokens_b{};
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_frame(
                  stream, {}, 0, 0, raw, tokens_b, second).error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    EXPECT_EQ(second, first);

    std::array<HuffmanDecodeTable, 35> tables{};
    std::array<LzssTypedToken, raw.size()> decode_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    const auto result = decode_lzss_contextual_blocked_huffman_frame(
        first, {stream, {}, 0, 0}, tables, decode_tokens, decoded);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     CapacityFailuresPreserveSerializedOutput) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 88> output{};
    std::ranges::fill(output, std::byte{0xcc});
    auto result = encode_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, {}, output);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  token_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));
    result = encode_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens,
        std::span<std::byte>{output}.first(87));
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     RejectsWorkspaceAliasingBeforeWrites) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    token_bytes[0] = std::byte{'A'};
    const auto marker = token_bytes[0];
    auto result = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{token_bytes}.first(1), tokens);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  overlapping_workspaces);
    EXPECT_EQ(token_bytes[0], marker);
    result = encode_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens, token_bytes);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  overlapping_workspaces);

    std::array<std::byte, 88> serialized_raw{};
    serialized_raw[0] = std::byte{'A'};
    result = encode_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{serialized_raw}.first(1), tokens,
        serialized_raw);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  overlapping_workspaces);
    EXPECT_EQ(serialized_raw[0], std::byte{'A'});
}

TEST(LzssContextualBlockedHuffmanFrameEncoder,
     RejectsStreamInputAndAggregateWorkspaceLimit) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens{};
    stream.max_code_length = 14;
    auto result = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::invalid_stream);
    stream = stream_for(raw.size() + 1);
    result = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::
                  input_size_mismatch);

    stream = stream_for(raw.size());
    result = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, raw, tokens);
    ASSERT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = raw.size()
        + result.token_encode.token_storage_size + result.serialized_size - 1;
    result = plan_lzss_contextual_blocked_huffman_frame(
        stream, limits, 0, 0, raw, tokens);
    EXPECT_EQ(result.error,
              LzssContextualBlockedHuffmanFrameEncodeError::workspace_limit);
}

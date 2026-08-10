#include "frame/lzss_contextual_tans_frame_encoder.hpp"

#include "frame/lzss_contextual_tans_frame_decoder.hpp"

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
using marc::entropy::internal::TansDecodeEntry;
using marc::entropy::internal::contextual_tans_decode_table_entries;
using marc::entropy::internal::contextual_tans_encode_table_entries;

[[nodiscard]] LzssContextualTansStreamHeader stream_for(
    const std::uint64_t original_size) {
    LzssContextualTansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> documented_literal_frame() {
    std::vector<std::byte> bytes(96);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01}; bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02}; bytes[32] = std::byte{0x02};
    bytes[36] = std::byte{0x1e};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

[[nodiscard]] std::vector<std::uint16_t> encode_tables() {
    return std::vector<std::uint16_t>(
        contextual_tans_encode_table_entries);
}

[[nodiscard]] std::vector<TansDecodeEntry> decode_tables() {
    return std::vector<TansDecodeEntry>(
        contextual_tans_decode_table_entries);
}

} // namespace

TEST(LzssContextualTansFrameEncoder, PlansAndEmitsDocumentedLiteralFrame) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xcc;
    auto tables = encode_tables();
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(result.serialized_size, 96U);
    EXPECT_EQ(result.descriptor_size, 30U);
    EXPECT_EQ(result.token_count, 1U);
    EXPECT_EQ(result.event_count, 2U);
    EXPECT_EQ(result.decision_count, 2U);
    EXPECT_EQ(result.payload_size, 2U);
    EXPECT_EQ(result.required_table_entries,
              contextual_tans_encode_table_entries);

    std::vector<std::byte> output(result.serialized_size + 1,
                                  std::byte{0xcc});
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, output);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    EXPECT_TRUE(std::ranges::equal(
        documented_literal_frame(),
        std::span<const std::byte>{output}.first(result.serialized_size)));
    EXPECT_EQ(output.back(), std::byte{0xcc});
    EXPECT_EQ(tokens[1].literal, 0xcc);
}

TEST(LzssContextualTansFrameEncoder, CompleteDecoderRecoversLiteral) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> encode_tokens{};
    auto encode_table_storage = encode_tables();
    std::vector<std::byte> frame(96);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, encode_tokens,
                  encode_table_storage, frame).error,
              LzssContextualTansFrameEncodeError::none);

    auto decode_table_storage = decode_tables();
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, decode_table_storage, decode_tokens,
        decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     RoundTripsMixedRawFrameDeterministically) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, raw.size()> tokens_a{};
    auto tables_a = encode_tables();
    const auto plan = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens_a, tables_a);
    ASSERT_EQ(plan.error, LzssContextualTansFrameEncodeError::none);
    std::vector<std::byte> first(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, tokens_a, tables_a, first).error,
              LzssContextualTansFrameEncodeError::none);

    std::array<LzssTypedToken, raw.size()> tokens_b{};
    auto tables_b = encode_tables();
    std::vector<std::byte> second(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_tans_frame(
                  stream, {}, 0, 0, raw, tokens_b, tables_b, second).error,
              LzssContextualTansFrameEncodeError::none);
    EXPECT_EQ(second, first);

    auto decoder_tables = decode_tables();
    std::array<LzssTypedToken, raw.size()> decode_tokens{};
    std::array<std::byte, raw.size()> decoded{};
    const auto result = decode_lzss_contextual_tans_frame(
        first, {stream, {}, 0, 0}, decoder_tables, decode_tokens, decoded);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssContextualTansFrameEncoder,
     CapacityFailuresPreserveSerializedOutput) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();
    std::vector<std::byte> output(96, std::byte{0xcc});

    auto result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw,
        std::span<LzssTypedToken>{tokens}.first(0), tables, output);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::token_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens,
        std::span<std::uint16_t>{tables}.first(tables.size() - 1), output);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::table_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables,
        std::span<std::byte>{output}.first(output.size() - 1));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::serialized_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssContextualTansFrameEncoder, RejectsWorkspaceAliasingBeforeWrites) {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    table_bytes[0] = std::byte{'A'};
    const auto table_marker = table_bytes[0];
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{table_bytes}.first(1), tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(table_bytes[0], table_marker);

    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    token_bytes[0] = std::byte{'A'};
    const auto token_marker = token_bytes[0];
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{token_bytes}.first(1), tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(token_bytes[0], token_marker);

    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, table_bytes.first(96));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables, token_bytes);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);

    std::array<std::byte, 96> serialized_raw{};
    serialized_raw[0] = std::byte{'A'};
    const auto raw_marker = serialized_raw[0];
    result = encode_lzss_contextual_tans_frame(
        stream, {}, 0, 0,
        std::span<const std::byte>{serialized_raw}.first(1), tokens, tables,
        serialized_raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(serialized_raw[0], raw_marker);
}

TEST(LzssContextualTansFrameEncoder,
     RejectsStreamInputAndAggregateWorkspaceLimit) {
    constexpr std::array raw{std::byte{'A'}};
    auto stream = stream_for(raw.size());
    std::array<LzssTypedToken, 1> tokens{};
    auto tables = encode_tables();
    stream.state_count = 2;
    auto result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::invalid_stream);

    stream = stream_for(2);
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::input_size_mismatch);

    stream = stream_for(raw.size());
    result = plan_lzss_contextual_tans_frame(
        stream, {}, 0, 0, raw, tokens, tables);
    ASSERT_EQ(result.error, LzssContextualTansFrameEncodeError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        raw.size() + result.token_encode.token_storage_size
        + contextual_tans_encode_table_entries * sizeof(std::uint16_t)
        + result.serialized_size - 1;
    result = plan_lzss_contextual_tans_frame(
        stream, limits, 0, 0, raw, tokens, tables);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameEncodeError::workspace_limit);
}

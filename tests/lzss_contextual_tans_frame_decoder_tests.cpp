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
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::ContextualTansDecodeError;
using marc::entropy::internal::ContextualTansFormatError;
using marc::entropy::internal::TansDecodeEntry;
using marc::entropy::internal::contextual_tans_decode_table_entries;

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> bytes(96);
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x02};
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

[[nodiscard]] LzssContextualTansStreamHeader stream_config() {
    LzssContextualTansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] std::vector<TansDecodeEntry> tables() {
    return std::vector<TansDecodeEntry>(
        contextual_tans_decode_table_entries);
}

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU};
}

} // namespace

TEST(LzssContextualTansFrameDecoder, DecodesSpecifiedFrameAtomically) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 2> tokens{token_marker(), token_marker()};
    std::array raw{std::byte{0xcc}, std::byte{0xcc}};

    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_table_entries,
              contextual_tans_decode_table_entries);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xcc);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xcc});
}

TEST(LzssContextualTansFrameDecoder, ConsumesOnePreflightedFrame) {
    auto frame = frame_vector();
    const auto expected = frame.size();
    frame.push_back(std::byte{0xa5});
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualTansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, expected);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssContextualTansFrameDecoder,
     DescriptorAndTruncationFailuresPreserveOutput) {
    auto frame = frame_vector();
    frame[84] = std::byte{1};
    frame[85] = std::byte{0};
    frame[86] = std::byte{0};
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};
    auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.descriptor_error,
              ContextualTansFormatError::trailing_data);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});

    frame = frame_vector();
    result = decode_lzss_contextual_tans_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              LzssContextualTansFramePreflightError::truncated_frame);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualTansFrameDecoder, CapacityFailuresPrecedeWrites) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    const TansDecodeEntry marker{0xa5a5, 0xa5, 0xa5};
    std::ranges::fill(table_storage, marker);
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0},
        std::span{table_storage}.first(table_storage.size() - 1), tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::table_output_too_small);
    result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage,
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::token_output_too_small);
    result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(table_storage, [](const auto& entry) {
        return entry.state_base == 0xa5a5 && entry.symbol == 0xa5
            && entry.bit_count == 0xa5;
    }));
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualTansFrameDecoder, RejectsWorkspaceAliasingBeforeWrites) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    const auto before = frame[0];
    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before);
}

TEST(LzssContextualTansFrameDecoder, EntropyFailurePreservesRawOutput) {
    auto frame = frame_vector();
    frame[94] = std::byte{1};
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};
    const auto result = decode_lzss_contextual_tans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualTansFrameDecodeError::token_decode_error);
    EXPECT_EQ(result.token_decode.entropy.error,
              ContextualTansDecodeError::invalid_terminal_state);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

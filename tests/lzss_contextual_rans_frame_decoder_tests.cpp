#include "frame/lzss_contextual_rans_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> bytes(9124);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01}; bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02}; bytes[32] = std::byte{0x08};
    bytes[36] = std::byte{0x5c}; bytes[37] = std::byte{0x23};
    bytes[64] = std::byte{0x02}; bytes[68] = std::byte{0x08};
    bytes[72] = std::byte{0x0c}; bytes[74] = std::byte{0x1f};
    bytes[76] = std::byte{0xa6}; bytes[77] = std::byte{0x11};
    bytes[80] = std::byte{0x00}; bytes[81] = std::byte{0x10};
    bytes[222] = std::byte{0x00}; bytes[223] = std::byte{0x10};
    bytes[9119] = std::byte{0x80};
    return bytes;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config() {
    LzssContextualRansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

} // namespace

TEST(LzssContextualRansFrameDecoder, DecodesSpecifiedFrameAtomically) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xcc;
    std::array raw{std::byte{0xcc}, std::byte{0xcc}};

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_table_entries,
              contextual_rans_decode_table_entries);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xcc);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder, ConsumesOnlyOnePreflightedFrame) {
    auto frame = frame_vector();
    const auto expected = frame.size();
    frame.push_back(std::byte{0xa5});
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, expected);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssContextualRansFrameDecoder, PreflightFailurePreservesAllWorkspace) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::ranges::fill(table_storage, marker);
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xcc, 0xccccccccU,
                       0xccccccccU}};
    std::array raw{std::byte{0xcc}};

    const auto result = decode_lzss_contextual_rans_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_TRUE(std::ranges::all_of(table_storage, [](const auto& entry) {
        return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
            && entry.symbol == 0xa5;
    }));
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder, EntropyFailurePreservesTokenAndRawOutput) {
    auto frame = frame_vector();
    std::ranges::fill(frame.begin() + 9116, frame.end(), std::byte{0});
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xcc, 0xccccccccU,
                       0xccccccccU}};
    std::array raw{std::byte{0xcc}};

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::token_decode_error);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[0].literal, 0xcc);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder, CapacityFailuresPrecedeAllWrites) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::ranges::fill(table_storage, marker);
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xcc, 1, 5}};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0},
        std::span{table_storage}.first(table_storage.size() - 1), tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::table_output_too_small);
    result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage,
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::token_output_too_small);
    result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::raw_output_too_small);
    EXPECT_TRUE(std::ranges::all_of(table_storage, [](const auto& entry) {
        return entry.cumulative == 0xa5a5 && entry.frequency == 0xa5a5
            && entry.symbol == 0xa5;
    }));
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansFrameDecoder, RejectsTableTokenRawOverlapBeforeWriting) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 2> token_storage{};
    auto token_bytes = std::as_writable_bytes(std::span{token_storage});
    std::ranges::fill(token_bytes, std::byte{0xcc});
    const auto before = token_bytes[0];

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage,
        std::span<LzssTypedToken>{token_storage}.first(1),
        token_bytes.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(token_bytes[0], before);
}

TEST(LzssContextualRansFrameDecoder, RejectsSerializedTableAliasingBeforeWriting) {
    const auto canonical = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    auto bytes = std::as_writable_bytes(std::span{table_storage});
    std::ranges::copy(canonical, bytes.begin());
    const auto before = std::array{bytes[0], bytes[1], bytes[2], bytes[3]};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_contextual_rans_frame(
        std::span<const std::byte>{bytes}.first(canonical.size()),
        {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
}

TEST(LzssContextualRansFrameDecoder, RejectsSerializedRawAliasingBeforeWriting) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    const auto before = frame[0];

    const auto result = decode_lzss_contextual_rans_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before);
}

#include "frame/lzss_contextual_rans_compact_frame_decoder.hpp"

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
using marc::entropy::internal::ContextualRansCompactFormatError;
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] std::vector<std::byte> compact_frame_vector() {
    std::vector<std::byte> bytes(98);
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x08};
    bytes[36] = std::byte{0x1a};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    std::ranges::copy(payload, bytes.begin() + 90);
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

[[nodiscard]] constexpr LzssTypedToken token_marker() {
    return {LzssTypedTokenKind::match, 0xcc, 0xccccccccU, 0xccccccccU};
}

} // namespace

TEST(LzssContextualRansCompactFrameDecoder, DecodesSpecifiedFrameAtomically) {
    const auto frame = compact_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 2> tokens{token_marker(), token_marker()};
    std::array raw{std::byte{0xcc}, std::byte{0xcc}};

    const auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_table_entries,
              contextual_rans_decode_table_entries);
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(result.token_decode.format_error,
              ContextualRansCompactFormatError::none);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xcc);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xcc});
}

TEST(LzssContextualRansCompactFrameDecoder, ConsumesOnePreflightedFrame) {
    auto frame = compact_frame_vector();
    const auto expected = frame.size();
    frame.push_back(std::byte{0xa5});
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    ASSERT_EQ(result.error, LzssContextualRansFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, expected);
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssContextualRansCompactFrameDecoder,
     DescriptorAndTruncationFailuresPreserveOutput) {
    auto frame = compact_frame_vector();
    frame[84] = std::byte{0x01};
    frame[85] = std::byte{0x00};
    frame[86] = std::byte{0x00};
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.descriptor_error,
              ContextualRansCompactFormatError::noncanonical_representation);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});

    frame = compact_frame_vector();
    result = decode_lzss_contextual_rans_compact_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              LzssContextualRansCompactFramePreflightError::truncated_frame);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansCompactFrameDecoder,
     RejectsDescriptorExtentsOutsideCompactBounds) {
    auto frame = compact_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    frame[36] = std::byte{0x16};
    auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.header_error,
              LzssContextualRansFrameHeaderError::contradictory_counts);

    frame = compact_frame_vector();
    frame[36] = std::byte{0x42};
    frame[37] = std::byte{0x23};
    result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.header_error,
              LzssContextualRansFrameHeaderError::contradictory_counts);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(raw[0], std::byte{0xcc});
}

TEST(LzssContextualRansCompactFrameDecoder, CapacityFailuresPrecedeWrites) {
    const auto frame = compact_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    const RansDecodeEntry marker{0xa5a5, 0xa5a5, 0xa5};
    std::ranges::fill(table_storage, marker);
    std::array<LzssTypedToken, 1> tokens{token_marker()};
    std::array raw{std::byte{0xcc}};

    auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0},
        std::span{table_storage}.first(table_storage.size() - 1), tokens, raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::table_output_too_small);
    result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage,
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::token_output_too_small);
    result = decode_lzss_contextual_rans_compact_frame(
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

TEST(LzssContextualRansCompactFrameDecoder,
     RejectsSerializedRawAliasingBeforeWrites) {
    auto frame = compact_frame_vector();
    const auto stream = stream_config();
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    const auto before = frame[0];

    const auto result = decode_lzss_contextual_rans_compact_frame(
        frame, {stream, {}, 0, 0}, table_storage, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssContextualRansFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before);
}

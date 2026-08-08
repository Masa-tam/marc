#include "frame/lzss_typed_context_frame_decoder.hpp"

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

[[nodiscard]] constexpr std::array<std::byte, 86> frame_vector() {
    std::array<std::byte, 86> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x06};
    bytes[36] = std::byte{0x10};
    bytes[64] = std::byte{0x02};
    bytes[68] = std::byte{0x06};
    bytes[72] = std::byte{0x1f};
    bytes[80] = std::byte{0x00};
    bytes[81] = std::byte{0x20};
    bytes[82] = std::byte{0x7f};
    bytes[83] = std::byte{0xff};
    bytes[84] = std::byte{0xbf};
    bytes[85] = std::byte{0x00};
    return bytes;
}

[[nodiscard]] constexpr TypedContextStreamHeader stream_config() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    return stream;
}

} // namespace

TEST(LzssTypedContextFrameDecoder, DecodesSpecifiedFrameAtomically) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xCC;
    std::array raw{std::byte{0xCC}, std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(result.token_decode.token_count, 1U);
    EXPECT_EQ(result.reconstruction.output_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xCC);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, ConsumesOnlyPreflightedFrameExtent) {
    constexpr auto canonical = frame_vector();
    std::vector<std::byte> frame(canonical.begin(), canonical.end());
    frame.push_back(std::byte{0xA5});
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, canonical.size());
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameDecoder, PreflightFailurePreservesAllWorkspace) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              TypedContextFramePreflightError::truncated_frame);
    EXPECT_EQ(result.required_token_count, 0U);
    EXPECT_EQ(result.required_raw_size, 0U);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, EntropyFailurePreservesAllWorkspace) {
    auto frame = frame_vector();
    frame[80] = std::byte{1};
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_decode_error);
    EXPECT_EQ(result.token_decode.error,
              marc::context::internal::LzssContextualRangeDecodeError::
                  entropy_error);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, CapacityFailuresPrecedeTokenWrites) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_output_too_small);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::raw_output_too_small);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, RejectsOverlappingWorkspacesBeforeWriting) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    std::ranges::fill(bytes, std::byte{0xCC});
    const auto before = bytes[0];

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{storage}.first(1), bytes.first(1));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(bytes[0], before);
}

TEST(LzssTypedContextFrameDecoder, RejectsSerializedTokenAliasingBeforeWriting) {
    constexpr auto canonical = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 9> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    std::ranges::copy(canonical, bytes.begin());
    const auto before = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]};
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{bytes}.first(canonical.size()),
        {stream, limits, 0, 0},
        std::span<LzssTypedToken>{storage}.first(1), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, RejectsSerializedRawAliasingBeforeWriting) {
    auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before_token = tokens[0];
    const auto before_byte = frame[0];

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{frame}, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before_byte);
    EXPECT_EQ(tokens[0].kind, before_token.kind);
    EXPECT_EQ(tokens[0].literal, before_token.literal);
    EXPECT_EQ(tokens[0].distance, before_token.distance);
    EXPECT_EQ(tokens[0].length, before_token.length);
}

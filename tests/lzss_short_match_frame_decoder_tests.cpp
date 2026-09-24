#include "frame/lzss_short_match_frame_decoder.hpp"

#include "core/endian.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] TypedContextStreamHeader short_stream() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 4;
    stream.original_size = 4;
    stream.dictionary.min_match_length = 3;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = 32;
    stream.dictionary_variant = 7;
    stream.context_variant = 6;
    return stream;
}

[[nodiscard]] std::array<std::byte, 87> hand_frame() {
    std::array<std::byte, 87> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    const std::span<std::byte> output{bytes};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 16, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(output, 20, std::uint32_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 24, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 28, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 32, std::uint32_t{7}));
    EXPECT_TRUE(marc::core::store_le(output, 36, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 64, std::uint32_t{5}));
    EXPECT_TRUE(marc::core::store_le(output, 68, std::uint32_t{7}));
    EXPECT_TRUE(marc::core::store_le(output, 72, std::uint16_t{32}));
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x30}, std::byte{0xbf}, std::byte{0xff},
        std::byte{0x9e}, std::byte{0x80}, std::byte{0x00}};
    for (std::size_t index = 0; index < payload.size(); ++index) {
        bytes[80 + index] = payload[index];
    }
    return bytes;
}

} // namespace

TEST(LzssShortMatchFrameDecoder, ReconstructsHandVectorPrivately) {
    const auto frame = hand_frame();
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw{};
    const auto result = decode_lzss_short_match_frame(frame, context, tokens,
                                                      raw);
    ASSERT_EQ(result.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_token_count, 2U);
    EXPECT_EQ(result.required_raw_size, 4U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[1].kind, LzssTypedTokenKind::match);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
    for (const auto value : raw) EXPECT_EQ(value, std::byte{0x61});
}

TEST(LzssShortMatchFrameDecoder, FailureNeverPublishesRawBytes) {
    auto frame = hand_frame();
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 7, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::literal, 8, 0, 0}};
    std::array<std::byte, 4> raw{};
    raw.fill(std::byte{0xcc});
    auto result = decode_lzss_short_match_frame(
        std::span<const std::byte>{frame}.first(86), context, tokens, raw);
    EXPECT_EQ(result.error, LzssShortMatchFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight_error,
              LzssShortMatchPreflightError::truncated_frame);
    EXPECT_EQ(result.serialized_consumed, 0U);
    frame[80] = std::byte{1};
    result = decode_lzss_short_match_frame(frame, context, tokens, raw);
    EXPECT_EQ(result.error, LzssShortMatchFrameDecodeError::token_decode_error);
    EXPECT_EQ(tokens[0].literal, 7U);
    EXPECT_EQ(tokens[1].literal, 8U);
    for (const auto value : raw) EXPECT_EQ(value, std::byte{0xcc});
}

TEST(LzssShortMatchFrameDecoder, ChecksPrivateCapacityAndOverlap) {
    auto frame = hand_frame();
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw{};
    raw.fill(std::byte{0xcc});
    auto result = decode_lzss_short_match_frame(
        frame, context, std::span<LzssTypedToken>{tokens}.first(1), raw);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameDecodeError::token_output_too_small);
    result = decode_lzss_short_match_frame(
        frame, context, tokens, std::span<std::byte>{raw}.first(3));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameDecodeError::raw_output_too_small);
    result = decode_lzss_short_match_frame(
        frame, context, tokens, std::span<std::byte>{frame}.first(4));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameDecodeError::overlapping_workspaces);
    result = decode_lzss_short_match_frame(
        frame, context, tokens,
        std::span<std::byte>{reinterpret_cast<std::byte*>(tokens.data()),
                             sizeof(tokens)}.first(4));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameDecodeError::overlapping_workspaces);
    for (const auto value : raw) EXPECT_EQ(value, std::byte{0xcc});
}

TEST(LzssShortMatchFrameDecoder, ResetsAtTheNextRawFrame) {
    auto frame = hand_frame();
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{frame}, 8,
                                     std::uint64_t{1}));
    auto stream = short_stream();
    stream.original_size = 8;
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 1, 4};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw{};
    const auto result = decode_lzss_short_match_frame(frame, context, tokens,
                                                      raw);
    ASSERT_EQ(result.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    for (const auto value : raw) EXPECT_EQ(value, std::byte{0x61});
}

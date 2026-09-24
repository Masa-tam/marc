#include "frame/lzss_short_match_frame_encoder.hpp"

#include "core/endian.hpp"
#include "frame/lzss_short_match_frame_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] TypedContextStreamHeader stream(const std::uint32_t frame_size,
                                               const std::uint64_t original_size) {
    return {frame_size, original_size, {65536, 3, 258, 0},
            typed_context_model_total, 32, 7, 1, 6};
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

constexpr std::array hand_tokens{
    LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
    LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};

} // namespace

TEST(LzssShortMatchFrameEncoder, ReproducesIndependentHandFrame) {
    const auto header = stream(4, 4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 5> operations{};
    const auto plan = plan_lzss_short_match_frame(
        header, limits, 0, 0, hand_tokens, operations);
    ASSERT_EQ(plan.error, LzssShortMatchFrameEncodeError::none);
    EXPECT_EQ(plan.serialized_size, 87U);
    EXPECT_EQ(plan.raw_size, 4U);
    EXPECT_EQ(plan.token_count, 2U);
    EXPECT_EQ(plan.operation_count, 5U);
    EXPECT_EQ(plan.decision_count, 5U);
    EXPECT_EQ(plan.payload_size, 7U);

    std::array<std::byte, 87> encoded{};
    const auto result = encode_lzss_short_match_frame(
        header, limits, 0, 0, hand_tokens, operations, encoded);
    ASSERT_EQ(result.error, LzssShortMatchFrameEncodeError::none);
    EXPECT_EQ(encoded, hand_frame());
    std::array<LzssTypedToken, 2> decoded_tokens{};
    std::array<std::byte, 4> raw{};
    const TypedContextFrameValidationContext context{header, limits, 0, 0};
    const auto decoded = decode_lzss_short_match_frame(
        encoded, context, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
}

TEST(LzssShortMatchFrameEncoder, EncodesLaterFrameAndMaximumLength) {
    constexpr std::array tokens{
        LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 4},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 4, 258}};
    const auto header = stream(263, 526);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 11> operations{};
    const auto plan = plan_lzss_short_match_frame(
        header, limits, 1, 263, tokens, operations);
    ASSERT_EQ(plan.error, LzssShortMatchFrameEncodeError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    const auto result = encode_lzss_short_match_frame(
        header, limits, 1, 263, tokens, operations, encoded);
    ASSERT_EQ(result.error, LzssShortMatchFrameEncodeError::none);
    std::array<LzssTypedToken, 3> decoded_tokens{};
    std::array<std::byte, 263> raw{};
    const TypedContextFrameValidationContext context{
        header, limits, 1, 263};
    const auto decoded = decode_lzss_short_match_frame(
        encoded, context, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssShortMatchFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, encoded.size());
    for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
}

TEST(LzssShortMatchFrameEncoder, RejectsInvalidAndShortOutputBeforeWrite) {
    auto tokens = hand_tokens;
    const auto header = stream(4, 4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 5> operations{};
    std::array<std::byte, 87> output{};
    output.fill(std::byte{0xcc});
    tokens[1].distance = 2;
    auto result = encode_lzss_short_match_frame(
        header, limits, 0, 0, tokens, operations, output);
    EXPECT_EQ(result.error, LzssShortMatchFrameEncodeError::context_error);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    tokens[1].distance = 1;
    result = encode_lzss_short_match_frame(
        header, limits, 0, 0, tokens,
        std::span<ModeledOperation>{operations}.first(4), output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::operation_workspace_too_small);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    result = encode_lzss_short_match_frame(
        header, limits, 0, 0, tokens, operations,
        std::span<std::byte>{output}.first(86));
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::serialized_output_too_small);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    result = encode_lzss_short_match_frame(
        header, limits, 1, 0, tokens, operations, output);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::invalid_frame_position);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortMatchFrameEncoder, RejectsOverlappingStorage) {
    auto tokens = hand_tokens;
    const auto header = stream(4, 4);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 5> operations{};
    const auto alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(tokens.data()), sizeof(tokens)};
    const auto result = encode_lzss_short_match_frame(
        header, limits, 0, 0, tokens, operations, alias);
    EXPECT_EQ(result.error,
              LzssShortMatchFrameEncodeError::overlapping_workspaces);
    EXPECT_EQ(tokens[0].literal, 0x61U);
    EXPECT_EQ(tokens[1].distance, 1U);
    EXPECT_EQ(tokens[1].length, 3U);
}

#include "frame/lzss_short_length_escape_stream_encoder.hpp"

#include "core/endian.hpp"
#include "frame/lzss_short_length_escape_stream_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

constexpr std::array tokens{
    LzssTypedToken{LzssTypedTokenKind::literal, 0x61, 0, 0},
    LzssTypedToken{LzssTypedTokenKind::match, 0, 1, 3}};

[[nodiscard]] TypedContextStreamHeader stream_for(
    const std::uint64_t raw_size) {
    return {4, raw_size, {65536, 3, 258, 0}, typed_context_model_total,
            32, 8, 1, 7};
}

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
expected_header(const std::uint64_t raw_size) {
    std::array<std::byte, typed_context_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    const std::span<std::byte> out{bytes};
    EXPECT_TRUE(marc::core::store_le(out, 4, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 8, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(out, 10, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(out, 12, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 14, std::uint16_t{8}));
    EXPECT_TRUE(marc::core::store_le(out, 16, std::uint16_t{3}));
    EXPECT_TRUE(marc::core::store_le(out, 18, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 20, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(out, 28, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 32, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 40, raw_size));
    EXPECT_TRUE(marc::core::store_le(out, 48, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 64, std::uint32_t{65536}));
    EXPECT_TRUE(marc::core::store_le(out, 68, std::uint32_t{3}));
    EXPECT_TRUE(marc::core::store_le(out, 72, std::uint32_t{258}));
    EXPECT_TRUE(marc::core::store_le(out, 80, typed_context_model_total));
    EXPECT_TRUE(marc::core::store_le(out, 84, std::uint16_t{32}));
    EXPECT_TRUE(marc::core::store_le(out, 96, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(out, 98, std::uint16_t{7}));
    return bytes;
}

} // namespace

TEST(LzssShortLengthEscapeStreamEncoder, EmitsExactEmptyStream) {
    const auto stream = stream_for(0);
    const auto limits = marc::core::DecoderLimits{};
    std::array<ModeledOperation, 6> operations{};
    const auto planned = plan_lzss_short_length_escape_stream(
        stream, limits, {}, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(planned.serialized_size, typed_context_stream_header_size);
    EXPECT_EQ(planned.frame_count, 0U);
    std::array<std::byte, typed_context_stream_header_size> output{};
    const auto encoded = encode_lzss_short_length_escape_stream(
        stream, limits, {}, operations, output);
    ASSERT_EQ(encoded.error, LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(output, expected_header(0));
    const auto decoded = decode_lzss_short_length_escape_stream(
        output, limits, {}, {}, {});
    EXPECT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
}

TEST(LzssShortLengthEscapeStreamEncoder, EmitsDeterministicTwoFrames) {
    const auto stream = stream_for(8);
    const auto limits = marc::core::DecoderLimits{};
    const std::array views{
        LzssShortLengthEscapeFrameTokens{tokens},
        LzssShortLengthEscapeFrameTokens{tokens}};
    std::array<ModeledOperation, 6> operations{};
    const auto planned = plan_lzss_short_length_escape_stream(
        stream, limits, views, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(planned.frame_count, 2U);
    std::vector<std::byte> encoded(planned.serialized_size);
    std::vector<std::byte> repeated(planned.serialized_size);
    const auto result = encode_lzss_short_length_escape_stream(
        stream, limits, views, operations, encoded);
    ASSERT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::none);
    const auto again = encode_lzss_short_length_escape_stream(
        stream, limits, views, operations, repeated);
    ASSERT_EQ(again.error, LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(encoded, repeated);
    EXPECT_TRUE(std::ranges::equal(
        std::span{encoded}.first(typed_context_stream_header_size),
        expected_header(8)));
    std::array<LzssTypedToken, 2> decoded_tokens{};
    std::array<std::byte, 4> frame_raw{};
    std::array<std::byte, 8> raw{};
    const auto decoded = decode_lzss_short_length_escape_stream(
        encoded, limits, decoded_tokens, frame_raw, raw);
    ASSERT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, encoded.size());
    EXPECT_EQ(decoded.frame_count, 2U);
    for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
    const auto old = decode_lzss_short_match_stream(
        encoded, limits, decoded_tokens, frame_raw, raw);
    EXPECT_EQ(old.error, LzssShortMatchStreamDecodeError::stream_header_error);
}

TEST(LzssShortLengthEscapeStreamEncoder, EmitsOneFrame) {
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    const std::array views{LzssShortLengthEscapeFrameTokens{tokens}};
    std::array<ModeledOperation, 6> operations{};
    const auto planned = plan_lzss_short_length_escape_stream(
        stream, limits, views, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeStreamEncodeError::none);
    std::vector<std::byte> encoded(planned.serialized_size);
    const auto result = encode_lzss_short_length_escape_stream(
        stream, limits, views, operations, encoded);
    ASSERT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::none);
    EXPECT_EQ(result.frame_count, 1U);
    std::array<LzssTypedToken, 2> decoded_tokens{};
    std::array<std::byte, 4> frame_raw{};
    std::array<std::byte, 4> raw{};
    const auto decoded = decode_lzss_short_length_escape_stream(
        encoded, limits, decoded_tokens, frame_raw, raw);
    ASSERT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
    EXPECT_EQ(decoded.frame_count, 1U);
    for (const auto byte : raw) EXPECT_EQ(byte, std::byte{0x61});
}

TEST(LzssShortLengthEscapeStreamEncoder, RejectsInvalidFramesBeforeWrite) {
    const auto stream = stream_for(8);
    const auto limits = marc::core::DecoderLimits{};
    auto damaged = tokens;
    damaged[1].distance = 2;
    const std::array views{
        LzssShortLengthEscapeFrameTokens{tokens},
        LzssShortLengthEscapeFrameTokens{damaged}};
    std::array<ModeledOperation, 6> operations{};
    std::array<std::byte, 512> output{};
    output.fill(std::byte{0xcc});
    auto result = encode_lzss_short_length_escape_stream(
        stream, limits, views, operations, output);
    EXPECT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    result = encode_lzss_short_length_escape_stream(
        stream, limits, std::span{views}.first(1), operations, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeStreamEncodeError::frame_count_mismatch);
    result = encode_lzss_short_length_escape_stream(
        stream_for(4), limits, views, operations, output);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeStreamEncodeError::frame_count_mismatch);
    auto wrong_stream = stream;
    wrong_stream.dictionary_variant = 7;
    result = encode_lzss_short_length_escape_stream(
        wrong_stream, limits, views, operations, output);
    EXPECT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::invalid_stream);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeStreamEncoder, RejectsShortAndOverlappingStorage) {
    const auto stream = stream_for(4);
    const auto limits = marc::core::DecoderLimits{};
    const std::array views{LzssShortLengthEscapeFrameTokens{tokens}};
    std::array<ModeledOperation, 6> operations{};
    std::array<std::byte, 512> output{};
    output.fill(std::byte{0xcc});
    const auto planned = plan_lzss_short_length_escape_stream(
        stream, limits, views, operations);
    ASSERT_EQ(planned.error, LzssShortLengthEscapeStreamEncodeError::none);
    auto result = encode_lzss_short_length_escape_stream(
        stream, limits, views, operations,
        std::span{output}.first(planned.serialized_size - 1));
    EXPECT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::output_too_small);
    result = encode_lzss_short_length_escape_stream(
        stream, limits, views, std::span{operations}.first(5), output);
    EXPECT_EQ(result.error, LzssShortLengthEscapeStreamEncodeError::frame_error);
    EXPECT_EQ(result.frame.error,
              LzssShortMatchFrameEncodeError::operation_workspace_too_small);
    auto mutable_tokens = tokens;
    std::array alias_views{LzssShortLengthEscapeFrameTokens{mutable_tokens}};
    const auto token_alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(mutable_tokens.data()),
        sizeof(mutable_tokens)};
    result = encode_lzss_short_length_escape_stream(
        stream, limits, alias_views, operations, token_alias);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeStreamEncodeError::overlapping_workspaces);
    const auto view_alias = std::span<std::byte>{
        reinterpret_cast<std::byte*>(alias_views.data()), sizeof(alias_views)};
    result = encode_lzss_short_length_escape_stream(
        stream, limits, alias_views, operations, view_alias);
    EXPECT_EQ(result.error,
              LzssShortLengthEscapeStreamEncodeError::overlapping_workspaces);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

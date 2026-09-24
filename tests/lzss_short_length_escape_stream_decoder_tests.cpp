#include "frame/lzss_short_length_escape_stream_decoder.hpp"

#include "context/lzss_field_context.hpp"
#include "core/endian.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
using marc::dictionary::internal::LzssTypedToken;

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
stream_header(const std::uint64_t original_size) {
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
    EXPECT_TRUE(marc::core::store_le(out, 40, original_size));
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

[[nodiscard]] std::vector<std::byte> frame(const std::uint64_t sequence) {
    const std::array<ModeledOperation, 6> operations{
        ModeledOperation{ModeledOperationKind::symbol, 0, 2, 0, 0},
        ModeledOperation{ModeledOperationKind::symbol, 3, 256, 0x61, 0},
        ModeledOperation{ModeledOperationKind::symbol, 1, 2, 1, 0},
        ModeledOperation{ModeledOperationKind::symbol, 21, 9, 8, 0},
        ModeledOperation{ModeledOperationKind::bypass_bits, 0, 0, 0, 1},
        ModeledOperation{ModeledOperationKind::symbol, 31, 17, 0, 0}};
    const auto limits = marc::core::DecoderLimits{};
    TypedContextRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::
        plan_lzss_short_match_range_operations(operations, limits, descriptor);
    EXPECT_EQ(plan.error, marc::entropy::internal::
                              ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> bytes(typed_context_frame_header_size
                                 + typed_context_range_descriptor_size
                                 + plan.payload_size);
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    const std::span<std::byte> out{bytes};
    EXPECT_TRUE(marc::core::store_le(out, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(out, 8, sequence));
    EXPECT_TRUE(marc::core::store_le(out, 16, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(out, 20, std::uint32_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 24, std::uint32_t{6}));
    EXPECT_TRUE(marc::core::store_le(out, 28, descriptor.decision_count));
    EXPECT_TRUE(marc::core::store_le(out, 32,
                                     static_cast<std::uint32_t>(plan.payload_size)));
    EXPECT_TRUE(marc::core::store_le(out, 36, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 64, descriptor.decision_count));
    EXPECT_TRUE(marc::core::store_le(out, 68,
                                     static_cast<std::uint32_t>(plan.payload_size)));
    EXPECT_TRUE(marc::core::store_le(out, 72, std::uint16_t{32}));
    TypedContextRangeDescriptor encoded_descriptor{};
    const auto encoded = marc::entropy::internal::
        encode_lzss_short_match_range_operations(
            operations, limits, out.subspan(80), encoded_descriptor);
    EXPECT_EQ(encoded.error, marc::entropy::internal::
                                 ContextualDynamicRangeEncodeError::none);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> stream(const std::size_t count) {
    const auto header = stream_header(4 * count);
    std::vector<std::byte> bytes(header.begin(), header.end());
    for (std::size_t index = 0; index < count; ++index) {
        const auto next = frame(index);
        bytes.insert(bytes.end(), next.begin(), next.end());
    }
    return bytes;
}

} // namespace

TEST(LzssShortLengthEscapeStreamDecoder, AcceptsEmptyOneAndTwoFrames) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    for (const std::size_t count : {0U, 1U, 2U}) {
        SCOPED_TRACE(count);
        const auto bytes = stream(count);
        std::array<std::byte, 8> output{};
        const auto decoded = decode_lzss_short_length_escape_stream(
            bytes, limits, tokens, raw_frame,
            std::span{output}.first(4 * count));
        ASSERT_EQ(decoded.error, LzssShortMatchStreamDecodeError::none);
        EXPECT_EQ(decoded.serialized_consumed, bytes.size());
        EXPECT_EQ(decoded.raw_produced, 4 * count);
        EXPECT_EQ(decoded.frame_count, count);
        for (std::size_t index = 0; index < 4 * count; ++index) {
            EXPECT_EQ(output[index], std::byte{0x61});
        }
        const auto old = decode_lzss_short_match_stream(
            bytes, limits, tokens, raw_frame,
            std::span{output}.first(4 * count));
        EXPECT_EQ(old.error, LzssShortMatchStreamDecodeError::stream_header_error);
    }
}

TEST(LzssShortLengthEscapeStreamDecoder, RejectsLateDamageAtomically) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    std::array<std::byte, 8> output{};
    const auto first = frame(0);
    auto bytes = stream(2);
    output.fill(std::byte{0xcc});
    bytes.push_back(std::byte{0});
    auto result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::trailing_data);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    bytes = stream(2);
    bytes.pop_back();
    result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.error_offset, typed_context_stream_header_size
                                   + first.size());
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
    bytes = stream(2);
    bytes[typed_context_stream_header_size + first.size() + 80] = std::byte{1};
    result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    for (const auto byte : output) EXPECT_EQ(byte, std::byte{0xcc});
}

TEST(LzssShortLengthEscapeStreamDecoder, ChecksSequenceLimitsAndOverlap) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    std::array<std::byte, 8> output{};
    auto bytes = stream(2);
    const auto frame_size = frame(0).size();
    EXPECT_TRUE(marc::core::store_le(
        std::span<std::byte>{bytes},
        typed_context_stream_header_size + frame_size + 8,
        std::uint64_t{2}));
    auto result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.preflight_error,
              LzssShortMatchPreflightError::unexpected_sequence);
    bytes = stream(2);
    result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, std::span{output}.first(7));
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::output_too_small);
    result = decode_lzss_short_length_escape_stream(
        bytes, limits, std::span{tokens}.first(1), raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.error,
              LzssShortMatchFrameDecodeError::token_output_too_small);
    result = decode_lzss_short_length_escape_stream(
        bytes, limits, tokens, raw_frame, std::span{bytes}.first(8));
    EXPECT_EQ(result.error,
              LzssShortMatchStreamDecodeError::overlapping_workspaces);
}

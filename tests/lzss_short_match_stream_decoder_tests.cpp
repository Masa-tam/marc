#include "frame/lzss_short_match_stream_decoder.hpp"

#include "core/endian.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;

[[nodiscard]] std::array<std::byte, 112> stream_header(
    const std::uint64_t original_size) {
    std::array<std::byte, 112> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    const std::span<std::byte> output{bytes};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 8, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 10, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(output, 12, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 14, std::uint16_t{7}));
    EXPECT_TRUE(marc::core::store_le(output, 16, std::uint16_t{3}));
    EXPECT_TRUE(marc::core::store_le(output, 18, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(output, 20, std::uint32_t{4}));
    EXPECT_TRUE(marc::core::store_le(output, 28, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 32, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 40, original_size));
    EXPECT_TRUE(marc::core::store_le(output, 48, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(output, 64, std::uint32_t{65536}));
    EXPECT_TRUE(marc::core::store_le(output, 68, std::uint32_t{3}));
    EXPECT_TRUE(marc::core::store_le(output, 72, std::uint32_t{258}));
    EXPECT_TRUE(marc::core::store_le(output, 80, typed_context_model_total));
    EXPECT_TRUE(marc::core::store_le(output, 84, std::uint16_t{32}));
    EXPECT_TRUE(marc::core::store_le(output, 96, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(output, 98, std::uint16_t{6}));
    return bytes;
}

[[nodiscard]] std::array<std::byte, 87> frame(
    const std::uint64_t sequence) {
    std::array<std::byte, 87> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    const std::span<std::byte> output{bytes};
    EXPECT_TRUE(marc::core::store_le(output, 4, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(output, 8, sequence));
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

[[nodiscard]] std::vector<std::byte> stream(
    const std::size_t frame_count) {
    const auto header = stream_header(4 * frame_count);
    std::vector<std::byte> bytes(header.begin(), header.end());
    for (std::size_t index = 0; index < frame_count; ++index) {
        const auto next = frame(index);
        bytes.insert(bytes.end(), next.begin(), next.end());
    }
    return bytes;
}

} // namespace

TEST(LzssShortMatchStreamDecoder, AcceptsExactEmptyStream) {
    const auto bytes = stream(0);
    const auto limits = marc::core::DecoderLimits{};
    const auto result = decode_lzss_short_match_stream(bytes, limits,
                                                        {}, {}, {});
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, 112U);
    EXPECT_EQ(result.raw_produced, 0U);
    EXPECT_EQ(result.frame_count, 0U);
}

TEST(LzssShortMatchStreamDecoder, DecodesOneAndTwoFrames) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    for (std::size_t count : {1U, 2U}) {
        const auto bytes = stream(count);
        std::array<std::byte, 8> output{};
        const auto result = decode_lzss_short_match_stream(
            bytes, limits, tokens, raw_frame,
            std::span<std::byte>{output}.first(4 * count));
        ASSERT_EQ(result.error, LzssShortMatchStreamDecodeError::none);
        EXPECT_EQ(result.serialized_consumed, bytes.size());
        EXPECT_EQ(result.raw_produced, 4 * count);
        EXPECT_EQ(result.frame_count, count);
        for (std::size_t index = 0; index < 4 * count; ++index) {
            EXPECT_EQ(output[index], std::byte{0x61});
        }
    }
}

TEST(LzssShortMatchStreamDecoder, RejectsTrailingAndTruncatedInputAtomically) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    std::array<std::byte, 8> output{};
    output.fill(std::byte{0xcc});
    auto bytes = stream(2);
    bytes.push_back(std::byte{0});
    auto result = decode_lzss_short_match_stream(bytes, limits, tokens,
                                                  raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::trailing_data);
    for (const auto value : output) EXPECT_EQ(value, std::byte{0xcc});
    bytes = stream(2);
    bytes.pop_back();
    result = decode_lzss_short_match_stream(bytes, limits, tokens,
                                             raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.preflight_error,
              LzssShortMatchPreflightError::truncated_frame);
    for (const auto value : output) EXPECT_EQ(value, std::byte{0xcc});
    bytes = stream(2);
    bytes[112 + 87 + 80] = std::byte{1};
    result = decode_lzss_short_match_stream(bytes, limits, tokens,
                                             raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.error,
              LzssShortMatchFrameDecodeError::token_decode_error);
    for (const auto value : output) EXPECT_EQ(value, std::byte{0xcc});
}

TEST(LzssShortMatchStreamDecoder, ChecksSequenceOutputAndWorkspace) {
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 4> raw_frame{};
    std::array<std::byte, 8> output{};
    auto bytes = stream(2);
    EXPECT_TRUE(marc::core::store_le(std::span<std::byte>{bytes}, 112 + 87 + 8,
                                     std::uint64_t{2}));
    auto result = decode_lzss_short_match_stream(bytes, limits, tokens,
                                                  raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.preflight_error,
              LzssShortMatchPreflightError::unexpected_sequence);
    bytes = stream(2);
    result = decode_lzss_short_match_stream(
        bytes, limits, tokens, raw_frame,
        std::span<std::byte>{output}.first(7));
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::output_too_small);
    result = decode_lzss_short_match_stream(
        bytes, limits, std::span<LzssTypedToken>{tokens}.first(1),
        raw_frame, output);
    EXPECT_EQ(result.error, LzssShortMatchStreamDecodeError::frame_error);
    EXPECT_EQ(result.frame.error,
              LzssShortMatchFrameDecodeError::token_output_too_small);
    result = decode_lzss_short_match_stream(
        bytes, limits, tokens, raw_frame,
        std::span<std::byte>{bytes}.first(8));
    EXPECT_EQ(result.error,
              LzssShortMatchStreamDecodeError::overlapping_workspaces);
}

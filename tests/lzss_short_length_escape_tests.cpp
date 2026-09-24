#include "context/lzss_short_length_escape.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace {

using namespace marc::context::internal;

TEST(LzssShortLengthEscape, EncodesHandCheckableBoundaries) {
    struct Vector {
        std::uint32_t length;
        std::uint8_t length_class;
        std::uint8_t bit_count;
        std::uint32_t extra;
    };
    constexpr std::array vectors{
        Vector{3, 8, 1, 0}, Vector{4, 8, 1, 1},
        Vector{5, 0, 0, 0}, Vector{6, 1, 1, 0},
        Vector{7, 1, 1, 1}, Vector{8, 2, 2, 0},
        Vector{132, 7, 7, 0}, Vector{258, 7, 7, 126}};
    for (const auto& vector : vectors) {
        SCOPED_TRACE(vector.length);
        const auto encoded = encode_lzss_short_length_escape(vector.length);
        ASSERT_EQ(encoded.error, LzssShortLengthEscapeError::none);
        EXPECT_EQ(encoded.length_class, vector.length_class);
        EXPECT_EQ(encoded.bit_count, vector.bit_count);
        EXPECT_EQ(encoded.extra, vector.extra);
        const auto decoded = decode_lzss_short_length_escape(
            encoded.length_class, encoded.bit_count, encoded.extra);
        ASSERT_EQ(decoded.error, LzssShortLengthEscapeError::none);
        EXPECT_EQ(decoded.length, vector.length);
    }
}

TEST(LzssShortLengthEscape, RoundTripsEveryPermittedLength) {
    for (std::uint32_t length = 3; length <= 258; ++length) {
        SCOPED_TRACE(length);
        const auto encoded = encode_lzss_short_length_escape(length);
        ASSERT_EQ(encoded.error, LzssShortLengthEscapeError::none);
        if (length >= 5) {
            const auto value = length - 4;
            std::uint8_t expected_class{};
            for (auto probe = value; probe >= 2; probe >>= 1) {
                ++expected_class;
            }
            EXPECT_EQ(encoded.length_class, expected_class);
            EXPECT_EQ(encoded.bit_count, expected_class);
        }
        const auto decoded = decode_lzss_short_length_escape(
            encoded.length_class, encoded.bit_count, encoded.extra);
        ASSERT_EQ(decoded.error, LzssShortLengthEscapeError::none);
        EXPECT_EQ(decoded.length, length);
    }
}

TEST(LzssShortLengthEscape, HasExactlyOneCanonicalCodePerLength) {
    std::array<bool, 259> seen{};
    std::uint32_t valid_count{};
    for (std::uint32_t length_class = 0; length_class <= 8;
         ++length_class) {
        const auto bits = static_cast<std::uint8_t>(
            length_class == 8 ? 1 : length_class);
        for (std::uint32_t extra = 0; extra < (1U << bits); ++extra) {
            const auto decoded = decode_lzss_short_length_escape(
                length_class, bits, extra);
            if (length_class == 7 && extra == 127) {
                EXPECT_EQ(decoded.error,
                          LzssShortLengthEscapeError::invalid_length);
                continue;
            }
            ASSERT_EQ(decoded.error, LzssShortLengthEscapeError::none);
            ASSERT_GE(decoded.length, 3U);
            ASSERT_LE(decoded.length, 258U);
            EXPECT_FALSE(seen[decoded.length]);
            seen[decoded.length] = true;
            ++valid_count;
            const auto encoded = encode_lzss_short_length_escape(
                decoded.length);
            ASSERT_EQ(encoded.error, LzssShortLengthEscapeError::none);
            EXPECT_EQ(encoded.length_class, length_class);
            EXPECT_EQ(encoded.bit_count, bits);
            EXPECT_EQ(encoded.extra, extra);
        }
    }
    EXPECT_EQ(valid_count, 256U);
    for (std::uint32_t length = 3; length <= 258; ++length) {
        EXPECT_TRUE(seen[length]);
    }
}

TEST(LzssShortLengthEscape, RejectsInvalidAndNoncanonicalFields) {
    for (const auto length : {0U, 2U, 259U, UINT32_MAX}) {
        EXPECT_EQ(encode_lzss_short_length_escape(length).error,
                  LzssShortLengthEscapeError::invalid_length);
    }
    EXPECT_EQ(decode_lzss_short_length_escape(9, 0, 0).error,
              LzssShortLengthEscapeError::invalid_class);
    EXPECT_EQ(decode_lzss_short_length_escape(UINT32_MAX, 0, 0).error,
              LzssShortLengthEscapeError::invalid_class);
    EXPECT_EQ(decode_lzss_short_length_escape(8, 0, 0).error,
              LzssShortLengthEscapeError::invalid_width);
    EXPECT_EQ(decode_lzss_short_length_escape(0, 1, 0).error,
              LzssShortLengthEscapeError::invalid_width);
    EXPECT_EQ(decode_lzss_short_length_escape(7, 8, 0).error,
              LzssShortLengthEscapeError::invalid_width);
    EXPECT_EQ(decode_lzss_short_length_escape(8, 1, 2).error,
              LzssShortLengthEscapeError::invalid_extra);
    EXPECT_EQ(decode_lzss_short_length_escape(3, 3, 8).error,
              LzssShortLengthEscapeError::invalid_extra);
    EXPECT_EQ(decode_lzss_short_length_escape(7, 7, 127).error,
              LzssShortLengthEscapeError::invalid_length);
}

} // namespace

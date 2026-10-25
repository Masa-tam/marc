#include "dictionary/lzss_prefix_hash.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {
using namespace marc::dictionary::internal;

TEST(LzssPrefixHash, MatchesHandCalculatedVectors) {
    struct Vector {
        std::array<std::byte, lzss_match_finder_prefix_size> prefix;
        std::uint32_t expected;
    };
    constexpr std::array vectors{
        Vector{{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
                std::byte{0}}, UINT32_C(0x00000000)},
        Vector{{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
                std::byte{0}}, UINT32_C(0x00102050)},
        Vector{{std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
                std::byte{0xff}, std::byte{0xff}}, UINT32_C(0x0f8cf287)},
        Vector{{std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3},
                std::byte{4}}, UINT32_C(0x00008876)},
    };

    for (const auto& vector : vectors) {
        const auto result = calculate_lzss_prefix_hash(vector.prefix, 0);
        ASSERT_TRUE(result.valid);
        EXPECT_EQ(result.value, vector.expected);
    }
}

TEST(LzssPrefixHash, SelectsTheRequestedFiveBytePrefix) {
    constexpr std::array input{
        std::byte{0xaa}, std::byte{0xbb}, std::byte{1}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0xcc}};

    const auto result = calculate_lzss_prefix_hash(input, 2);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.value, UINT32_C(0x00102050));
}

TEST(LzssPrefixHash, RejectsEveryShortOrOutOfRangeRequest) {
    constexpr std::array input{
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3},
        std::byte{4}};

    for (std::size_t size = 0; size < lzss_match_finder_prefix_size;
         ++size) {
        const auto result = calculate_lzss_prefix_hash(
            std::span<const std::byte>{input}.first(size), 0);
        EXPECT_FALSE(result.valid) << size;
        EXPECT_EQ(result.value, 0U) << size;
    }
    for (std::size_t position = 1; position <= input.size() + 1;
         ++position) {
        const auto result = calculate_lzss_prefix_hash(input, position);
        EXPECT_FALSE(result.valid) << position;
        EXPECT_EQ(result.value, 0U) << position;
    }
}

TEST(LzssPrefixHash, AcceptsEveryByteAtEveryPrefixOffset) {
    std::array<std::byte, lzss_match_finder_prefix_size> input{};
    for (std::size_t offset = 0; offset < input.size(); ++offset) {
        for (std::uint16_t value = 0; value <= UINT8_MAX; ++value) {
            input.fill(std::byte{0});
            input[offset] = static_cast<std::byte>(value);
            const auto first = calculate_lzss_prefix_hash(input, 0);
            const auto second = calculate_lzss_prefix_hash(input, 0);
            ASSERT_TRUE(first.valid) << offset << ' ' << value;
            EXPECT_EQ(first.value, second.value) << offset << ' ' << value;
        }
    }
}

TEST(LzssPrefixHash, PreservesTheKnownExactCollision) {
    constexpr std::array first{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}};
    constexpr std::array second{
        std::byte{0}, std::byte{0x20}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}};

    const auto first_result = calculate_lzss_prefix_hash(first, 0);
    const auto second_result = calculate_lzss_prefix_hash(second, 0);

    ASSERT_TRUE(first_result.valid);
    ASSERT_TRUE(second_result.valid);
    EXPECT_EQ(first_result.value, UINT32_C(0x00102b1f));
    EXPECT_EQ(second_result.value, first_result.value);
}

} // namespace

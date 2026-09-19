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

TEST(LzssPrefixHashMnemonicMixerV1, MatchesFixedVectors) {
    struct Vector {
        std::array<std::byte, lzss_match_finder_prefix_size> prefix;
        std::uint32_t expected;
    };
    constexpr std::array vectors{
        Vector{{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
                std::byte{0}}, UINT32_C(0x00000000)},
        Vector{{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                std::byte{5}}, UINT32_C(0x4d6ccd81)},
        Vector{{std::byte{0x41}, std::byte{0x42}, std::byte{0x43},
                std::byte{0x44}, std::byte{0x45}}, UINT32_C(0x892c8ac2)},
        Vector{{std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
                std::byte{0xff}, std::byte{0xff}}, UINT32_C(0x200c8d40)},
        Vector{{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
                std::byte{0x59}}, UINT32_C(0x8e1546d4)},
        Vector{{std::byte{0}, std::byte{0x20}, std::byte{0},
                std::byte{0x58}, std::byte{0x59}}, UINT32_C(0x57938f46)},
    };

    for (const auto& vector : vectors) {
        const auto result = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
            vector.prefix, 0);
        ASSERT_TRUE(result.valid);
        EXPECT_EQ(result.value, vector.expected);
    }
}

TEST(LzssPrefixHashMnemonicMixerV1, SelectsRequestedPrefixAndRejectsBounds) {
    constexpr std::array input{
        std::byte{0xaa}, std::byte{0xbb}, std::byte{1}, std::byte{2},
        std::byte{3}, std::byte{4}, std::byte{5}, std::byte{0xcc}};

    const auto selected = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
        input, 2);
    ASSERT_TRUE(selected.valid);
    EXPECT_EQ(selected.value, UINT32_C(0x4d6ccd81));

    for (std::size_t size = 0; size < lzss_match_finder_prefix_size;
         ++size) {
        const auto result = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
            std::span<const std::byte>{input}.first(size), 0);
        EXPECT_FALSE(result.valid) << size;
        EXPECT_EQ(result.value, 0U) << size;
    }
    for (std::size_t position =
             input.size() - (lzss_match_finder_prefix_size - 1U);
         position <= input.size() + 1; ++position) {
        const auto result = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
            input, position);
        EXPECT_FALSE(result.valid) << position;
        EXPECT_EQ(result.value, 0U) << position;
    }
}

TEST(LzssPrefixHashMnemonicMixerV1,
     SeparatesLegacyCollisionFromFourBucketsUpward) {
    constexpr std::array first{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}};
    constexpr std::array second{
        std::byte{0}, std::byte{0x20}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}};

    const auto first_result = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
        first, 0);
    const auto second_result = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
        second, 0);
    ASSERT_TRUE(first_result.valid);
    ASSERT_TRUE(second_result.valid);

    for (std::size_t bucket_count = 1;
         bucket_count <= lzss_match_finder_max_bucket_count;
         bucket_count *= 2U) {
        const auto mask = static_cast<std::uint32_t>(bucket_count - 1U);
        const auto first_bucket = first_result.value & mask;
        const auto second_bucket = second_result.value & mask;
        if (bucket_count <= 2U) {
            EXPECT_EQ(first_bucket, second_bucket) << bucket_count;
        } else {
            EXPECT_NE(first_bucket, second_bucket) << bucket_count;
        }
    }
}

TEST(LzssPrefixHashMnemonicMixerV1, IsDeterministicForEveryByteAndOffset) {
    std::array<std::byte, lzss_match_finder_prefix_size> input{};
    for (std::size_t offset = 0; offset < input.size(); ++offset) {
        for (std::uint16_t value = 0; value <= UINT8_MAX; ++value) {
            input.fill(std::byte{0});
            input[offset] = static_cast<std::byte>(value);
            const auto first = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
                input, 0);
            const auto second = calculate_lzss_prefix_hash_mnemonic_mixer_v1(
                input, 0);
            ASSERT_TRUE(first.valid) << offset << ' ' << value;
            EXPECT_EQ(first.value, second.value) << offset << ' ' << value;
        }
    }
}

} // namespace

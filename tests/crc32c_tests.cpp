#include "core/crc32c.hpp"
#include "core/hash_tap.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {

[[nodiscard]] constexpr std::array<std::byte, 4> little_endian(
    const std::uint32_t value) noexcept {
    return {
        static_cast<std::byte>(value & 0xffU),
        static_cast<std::byte>((value >> 8U) & 0xffU),
        static_cast<std::byte>((value >> 16U) & 0xffU),
        static_cast<std::byte>((value >> 24U) & 0xffU),
    };
}

[[nodiscard]] std::span<const std::byte> as_bytes(
    const std::string_view text) noexcept {
    return std::as_bytes(std::span{text.data(), text.size()});
}

[[nodiscard]] std::array<std::byte, 4> digest(
    const std::span<const std::byte> input) {
    marc::core::Crc32c algorithm;
    EXPECT_TRUE(algorithm.update(input));
    std::array<std::byte, 4> output{};
    EXPECT_TRUE(algorithm.finalize(output));
    return output;
}

} // namespace

TEST(Crc32cTest, ReportsStableMetadataAndEmptyDigest) {
    marc::core::Crc32c algorithm;
    EXPECT_EQ(algorithm.algorithm_id(), marc::core::crc32c_algorithm_id);
    EXPECT_EQ(algorithm.digest_size(), marc::core::crc32c_digest_size);

    std::array<std::byte, 4> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, little_endian(0));
}

TEST(Crc32cTest, MatchesAsciiCheckVectorAndLittleEndianDigest) {
    constexpr std::string_view check = "123456789";
    EXPECT_EQ(digest(as_bytes(check)), little_endian(0xe3069283U));
}

TEST(Crc32cTest, MatchesPublishedBinaryCheckVectors) {
    std::array<std::byte, 32> zeros{};
    EXPECT_EQ(digest(zeros), little_endian(0x8a9136aaU));

    std::array<std::byte, 32> ascending{};
    for (std::size_t index = 0; index < ascending.size(); ++index) {
        ascending[index] = static_cast<std::byte>(index);
    }
    EXPECT_EQ(digest(ascending), little_endian(0x46dd794eU));
}

TEST(Crc32cTest, IsIndependentOfEveryCheckVectorSplit) {
    constexpr std::string_view check = "123456789";
    const auto bytes = as_bytes(check);
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        marc::core::Crc32c algorithm;
        ASSERT_TRUE(algorithm.update(bytes.first(split)));
        ASSERT_TRUE(algorithm.update(bytes.subspan(split)));
        std::array<std::byte, 4> output{};
        ASSERT_TRUE(algorithm.finalize(output));
        EXPECT_EQ(output, little_endian(0xe3069283U)) << "split=" << split;
    }
}

TEST(Crc32cTest, ResetRestoresInitialStateAfterSnapshot) {
    marc::core::Crc32c algorithm;
    ASSERT_TRUE(algorithm.update(as_bytes("123456789")));
    std::array<std::byte, 4> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, little_endian(0xe3069283U));
    std::array<std::byte, 4> repeated{};
    ASSERT_TRUE(algorithm.finalize(repeated));
    EXPECT_EQ(repeated, output);

    algorithm.reset();
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, little_endian(0));
}

TEST(Crc32cTest, ComposesWithHashTapCommittedPrefix) {
    constexpr std::string_view input = "123456789-not-committed";
    marc::core::Crc32c algorithm;
    marc::core::HashTap tap{algorithm};
    ASSERT_EQ(tap.commit(as_bytes(input), 9), marc::core::HashTapStatus::ok);

    std::array<std::byte, 4> output{};
    ASSERT_EQ(tap.finalize(output), marc::core::HashTapStatus::ok);
    EXPECT_EQ(output, little_endian(0xe3069283U));
    EXPECT_EQ(tap.total_committed(), 9U);
}

TEST(Crc32cTest, WrongDigestSizeDoesNotModifyOutput) {
    marc::core::Crc32c algorithm;
    ASSERT_TRUE(algorithm.update(as_bytes("123456789")));

    std::array<std::byte, 3> short_output{};
    std::ranges::fill(short_output, std::byte{0xa5});
    const auto original_short = short_output;
    EXPECT_FALSE(algorithm.finalize(short_output));
    EXPECT_EQ(short_output, original_short);

    std::array<std::byte, 5> long_output{};
    std::ranges::fill(long_output, std::byte{0x5a});
    const auto original_long = long_output;
    EXPECT_FALSE(algorithm.finalize(long_output));
    EXPECT_EQ(long_output, original_long);

    std::array<std::byte, 4> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, little_endian(0xe3069283U));
}

#include "core/sha256.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace {

[[nodiscard]] constexpr unsigned int hex_nibble(const char value) noexcept {
    return value >= '0' && value <= '9'
               ? static_cast<unsigned int>(value - '0')
               : static_cast<unsigned int>(value - 'a' + 10);
}

[[nodiscard]] constexpr std::array<std::byte, 32> digest_bytes(
    const std::string_view hex) noexcept {
    std::array<std::byte, 32> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            (hex_nibble(hex[index * 2U]) << 4U) |
            hex_nibble(hex[index * 2U + 1U]));
    }
    return result;
}

[[nodiscard]] std::span<const std::byte> as_bytes(
    const std::string_view text) noexcept {
    return std::as_bytes(std::span{text.data(), text.size()});
}

[[nodiscard]] std::array<std::byte, 32> hash(
    const std::span<const std::byte> input) {
    marc::core::Sha256 algorithm;
    EXPECT_TRUE(algorithm.update(input));
    std::array<std::byte, 32> output{};
    EXPECT_TRUE(algorithm.finalize(output));
    return output;
}

constexpr std::string_view empty_digest =
    "e3b0c44298fc1c149afbf4c8996fb924"
    "27ae41e4649b934ca495991b7852b855";
constexpr std::string_view abc_digest =
    "ba7816bf8f01cfea414140de5dae2223"
    "b00361a396177a9cb410ff61f20015ad";
constexpr std::string_view multi_block_message =
    "abcdbcdecdefdefgefghfghighijhijk"
    "ijkljklmklmnlmnomnopnopq";
constexpr std::string_view multi_block_digest =
    "248d6a61d20638b8e5c026930c3e6039"
    "a33ce45964ff2167f6ecedd419db06c1";

} // namespace

TEST(Sha256Test, ReportsStableMetadataAndEmptyDigest) {
    marc::core::Sha256 algorithm;
    EXPECT_EQ(algorithm.algorithm_id(), marc::core::sha256_algorithm_id);
    EXPECT_EQ(algorithm.digest_size(), marc::core::sha256_digest_size);

    std::array<std::byte, 32> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, digest_bytes(empty_digest));
}

TEST(Sha256Test, MatchesAbcStandardDigestBytes) {
    EXPECT_EQ(hash(as_bytes("abc")), digest_bytes(abc_digest));
}

TEST(Sha256Test, IsIndependentOfEveryMultiBlockCheckVectorSplit) {
    const auto bytes = as_bytes(multi_block_message);
    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        marc::core::Sha256 algorithm;
        ASSERT_TRUE(algorithm.update(bytes.first(split)));
        ASSERT_TRUE(algorithm.update(bytes.subspan(split)));
        std::array<std::byte, 32> output{};
        ASSERT_TRUE(algorithm.finalize(output));
        EXPECT_EQ(output, digest_bytes(multi_block_digest))
            << "split=" << split;
    }
}

TEST(Sha256Test, HandlesOneByteUpdatesAcrossManyBlocks) {
    std::array<std::byte, 1000> input{};
    std::ranges::fill(input, std::byte{0x61});
    marc::core::Sha256 algorithm;
    for (const std::byte byte : input) {
        ASSERT_TRUE(algorithm.update(std::span{&byte, std::size_t{1}}));
    }
    std::array<std::byte, 32> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, digest_bytes(
        "41edece42d63e8d9bf515a9ba6932e1c"
        "20cbc9f5a5d134645adb5db1b9737ea3"));
}

TEST(Sha256Test, FinalSnapshotsRepeatAndResetRestoresInitialState) {
    marc::core::Sha256 algorithm;
    ASSERT_TRUE(algorithm.update(as_bytes("abc")));
    std::array<std::byte, 32> first{};
    std::array<std::byte, 32> second{};
    ASSERT_TRUE(algorithm.finalize(first));
    ASSERT_TRUE(algorithm.finalize(second));
    EXPECT_EQ(first, digest_bytes(abc_digest));
    EXPECT_EQ(second, first);

    algorithm.reset();
    ASSERT_TRUE(algorithm.finalize(first));
    EXPECT_EQ(first, digest_bytes(empty_digest));
}

TEST(Sha256Test, SnapshotDoesNotPreventFurtherUpdates) {
    marc::core::Sha256 algorithm;
    ASSERT_TRUE(algorithm.update(as_bytes("a")));
    std::array<std::byte, 32> snapshot{};
    ASSERT_TRUE(algorithm.finalize(snapshot));
    ASSERT_TRUE(algorithm.update(as_bytes("bc")));
    std::array<std::byte, 32> completed{};
    ASSERT_TRUE(algorithm.finalize(completed));
    EXPECT_EQ(completed, digest_bytes(abc_digest));
}

TEST(Sha256Test, WrongDigestSizeDoesNotModifyOutput) {
    marc::core::Sha256 algorithm;
    ASSERT_TRUE(algorithm.update(as_bytes("abc")));

    std::array<std::byte, 31> short_output{};
    std::ranges::fill(short_output, std::byte{0xa5});
    const auto original_short = short_output;
    EXPECT_FALSE(algorithm.finalize(short_output));
    EXPECT_EQ(short_output, original_short);

    std::array<std::byte, 33> long_output{};
    std::ranges::fill(long_output, std::byte{0x5a});
    const auto original_long = long_output;
    EXPECT_FALSE(algorithm.finalize(long_output));
    EXPECT_EQ(long_output, original_long);

    std::array<std::byte, 32> output{};
    ASSERT_TRUE(algorithm.finalize(output));
    EXPECT_EQ(output, digest_bytes(abc_digest));
}

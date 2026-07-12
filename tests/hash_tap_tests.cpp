#include "core/hash_tap.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

class SumHash final : public marc::core::IHashAlgorithm {
public:
    void reset() noexcept override {
        sum_ = 0;
        ++reset_count_;
    }

    [[nodiscard]] bool update(
        const std::span<const std::byte> bytes) noexcept override {
        if (fail_update_) {
            return false;
        }
        for (const auto value : bytes) {
            sum_ += std::to_integer<std::uint8_t>(value);
        }
        update_bytes_ += bytes.size();
        return true;
    }

    [[nodiscard]] bool finalize(
        const std::span<std::byte> output) noexcept override {
        if (fail_finalize_ || output.size() != digest_size()) {
            return false;
        }
        for (std::size_t index = 0; index < output.size(); ++index) {
            output[index] = static_cast<std::byte>(sum_ >> (index * 8U));
        }
        return true;
    }

    [[nodiscard]] std::size_t digest_size() const noexcept override {
        return 8;
    }

    [[nodiscard]] marc::core::HashAlgorithmId algorithm_id()
        const noexcept override {
        return 0x74657374;
    }

    std::uint64_t sum_{};
    std::size_t update_bytes_{};
    std::size_t reset_count_{};
    bool fail_update_{};
    bool fail_finalize_{};
};

[[nodiscard]] std::uint64_t decode_digest(
    const std::array<std::byte, 8>& digest) {
    std::uint64_t value{};
    for (std::size_t index = 0; index < digest.size(); ++index) {
        value |= static_cast<std::uint64_t>(
                     std::to_integer<std::uint8_t>(digest[index]))
                 << (index * 8U);
    }
    return value;
}

} // namespace

using marc::core::HashTapState;
using marc::core::HashTapStatus;

TEST(HashTapTest, HashesOnlyCommittedPrefix) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    const std::array bytes{
        std::byte{1}, std::byte{2}, std::byte{100}, std::byte{200}};

    ASSERT_EQ(tap.commit(bytes, 2), HashTapStatus::ok);
    EXPECT_EQ(tap.total_committed(), 2U);
    EXPECT_EQ(algorithm.update_bytes_, 2U);
    EXPECT_EQ(algorithm.sum_, 3U);
}

TEST(HashTapTest, DigestIsIndependentOfChunking) {
    const std::array bytes{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};

    for (std::size_t split = 0; split <= bytes.size(); ++split) {
        SumHash algorithm;
        marc::core::HashTap tap{algorithm};
        ASSERT_EQ(tap.commit(std::span<const std::byte>{bytes}.first(split),
                             split),
                  HashTapStatus::ok);
        ASSERT_EQ(tap.commit(std::span<const std::byte>{bytes}.subspan(split),
                             bytes.size() - split),
                  HashTapStatus::ok);
        std::array<std::byte, 8> digest{};
        ASSERT_EQ(tap.finalize(digest), HashTapStatus::ok);
        EXPECT_EQ(decode_digest(digest), 36U) << "split=" << split;
    }
}

TEST(HashTapTest, HandlesEmptyScope) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    ASSERT_EQ(tap.commit({}, 0), HashTapStatus::ok);
    std::array<std::byte, 8> digest{};
    ASSERT_EQ(tap.finalize(digest), HashTapStatus::ok);
    EXPECT_EQ(decode_digest(digest), 0U);
    EXPECT_EQ(tap.total_committed(), 0U);
}

TEST(HashTapTest, RejectsCommitBeyondAvailableBytesWithoutMutation) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    const std::array bytes{std::byte{1}, std::byte{2}};
    EXPECT_EQ(tap.commit(bytes, 3), HashTapStatus::invalid_argument);
    EXPECT_EQ(tap.state(), HashTapState::running);
    EXPECT_EQ(tap.total_committed(), 0U);
    EXPECT_EQ(algorithm.update_bytes_, 0U);
}

TEST(HashTapTest, AllowsDigestSizeMistakeToBeRetried) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    std::array<std::byte, 7> short_digest{};
    EXPECT_EQ(tap.finalize(short_digest), HashTapStatus::invalid_argument);
    EXPECT_EQ(tap.state(), HashTapState::running);

    std::array<std::byte, 8> digest{};
    EXPECT_EQ(tap.finalize(digest), HashTapStatus::ok);
    EXPECT_EQ(tap.state(), HashTapState::finalized);
}

TEST(HashTapTest, FinalizationIsTerminalUntilReset) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    std::array<std::byte, 8> digest{};
    ASSERT_EQ(tap.finalize(digest), HashTapStatus::ok);
    EXPECT_EQ(tap.finalize(digest), HashTapStatus::already_finalized);
    EXPECT_EQ(tap.commit({}, 0), HashTapStatus::already_finalized);

    tap.reset();
    EXPECT_EQ(tap.state(), HashTapState::running);
    EXPECT_EQ(tap.total_committed(), 0U);
    EXPECT_GE(algorithm.reset_count_, 2U);
}

TEST(HashTapTest, AlgorithmUpdateFailureIsTerminal) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    algorithm.fail_update_ = true;
    const std::array bytes{std::byte{1}};
    EXPECT_EQ(tap.commit(bytes, 1), HashTapStatus::algorithm_failure);
    EXPECT_EQ(tap.state(), HashTapState::error);
    EXPECT_EQ(tap.commit(bytes, 1), HashTapStatus::error_state);
}

TEST(HashTapTest, AlgorithmFinalizeFailureIsTerminal) {
    SumHash algorithm;
    marc::core::HashTap tap{algorithm};
    algorithm.fail_finalize_ = true;
    std::array<std::byte, 8> digest{};
    EXPECT_EQ(tap.finalize(digest), HashTapStatus::algorithm_failure);
    EXPECT_EQ(tap.state(), HashTapState::error);
    EXPECT_EQ(tap.finalize(digest), HashTapStatus::error_state);
}

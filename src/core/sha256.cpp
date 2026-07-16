#include "core/sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::core {
namespace {

constexpr std::array<std::uint32_t, 8> initial_state{
    0x6a09e667U,
    0xbb67ae85U,
    0x3c6ef372U,
    0xa54ff53aU,
    0x510e527fU,
    0x9b05688cU,
    0x1f83d9abU,
    0x5be0cd19U,
};

constexpr std::array<std::uint32_t, 64> round_constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::uint64_t maximum_message_bytes =
    std::numeric_limits<std::uint64_t>::max() / 8U;

[[nodiscard]] constexpr std::uint32_t choose(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

[[nodiscard]] constexpr std::uint32_t majority(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

[[nodiscard]] constexpr std::uint32_t big_sigma_zero(
    const std::uint32_t value) noexcept {
    return std::rotr(value, 2) ^ std::rotr(value, 13) ^ std::rotr(value, 22);
}

[[nodiscard]] constexpr std::uint32_t big_sigma_one(
    const std::uint32_t value) noexcept {
    return std::rotr(value, 6) ^ std::rotr(value, 11) ^ std::rotr(value, 25);
}

[[nodiscard]] constexpr std::uint32_t small_sigma_zero(
    const std::uint32_t value) noexcept {
    return std::rotr(value, 7) ^ std::rotr(value, 18) ^ (value >> 3U);
}

[[nodiscard]] constexpr std::uint32_t small_sigma_one(
    const std::uint32_t value) noexcept {
    return std::rotr(value, 17) ^ std::rotr(value, 19) ^ (value >> 10U);
}

[[nodiscard]] constexpr std::uint32_t load_be32(
    const std::span<const std::byte> input,
    const std::size_t offset) noexcept {
    return (std::to_integer<std::uint32_t>(input[offset]) << 24U) |
           (std::to_integer<std::uint32_t>(input[offset + 1]) << 16U) |
           (std::to_integer<std::uint32_t>(input[offset + 2]) << 8U) |
           std::to_integer<std::uint32_t>(input[offset + 3]);
}

} // namespace

void Sha256::reset() noexcept {
    state_ = initial_state;
    block_.fill(std::byte{0});
    total_bytes_ = 0;
    block_size_ = 0;
}

bool Sha256::update(const std::span<const std::byte> bytes) noexcept {
    if (bytes.size() > maximum_message_bytes - total_bytes_) {
        return false;
    }
    total_bytes_ += static_cast<std::uint64_t>(bytes.size());

    std::size_t offset{};
    if (block_size_ != 0) {
        const std::size_t copied =
            std::min(block_.size() - block_size_, bytes.size());
        std::copy_n(bytes.begin(), copied, block_.begin() + block_size_);
        block_size_ += copied;
        offset += copied;
        if (block_size_ == block_.size()) {
            transform(block_);
            block_size_ = 0;
        } else {
            return true;
        }
    }

    while (bytes.size() - offset >= block_.size()) {
        transform(bytes.subspan(offset, block_.size()));
        offset += block_.size();
    }

    const std::size_t remaining = bytes.size() - offset;
    std::copy_n(bytes.begin() + offset, remaining, block_.begin());
    block_size_ = remaining;
    return true;
}

bool Sha256::finalize(
    const std::span<std::byte> digest_output) noexcept {
    if (digest_output.size() != sha256_digest_size) {
        return false;
    }
    Sha256 snapshot = *this;
    snapshot.finish(digest_output);
    return true;
}

std::size_t Sha256::digest_size() const noexcept {
    return sha256_digest_size;
}

HashAlgorithmId Sha256::algorithm_id() const noexcept {
    return sha256_algorithm_id;
}

void Sha256::transform(const std::span<const std::byte> block) noexcept {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16; ++index) {
        schedule[index] = load_be32(block, index * 4U);
    }
    for (std::size_t index = 16; index < schedule.size(); ++index) {
        schedule[index] = small_sigma_one(schedule[index - 2]) +
                          schedule[index - 7] +
                          small_sigma_zero(schedule[index - 15]) +
                          schedule[index - 16];
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (std::size_t index = 0; index < schedule.size(); ++index) {
        const std::uint32_t temporary_one =
            h + big_sigma_one(e) + choose(e, f, g) +
            round_constants[index] + schedule[index];
        const std::uint32_t temporary_two =
            big_sigma_zero(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temporary_one;
        d = c;
        c = b;
        b = a;
        a = temporary_one + temporary_two;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::finish(const std::span<std::byte> digest_output) noexcept {
    const std::uint64_t bit_length = total_bytes_ * 8U;
    block_[block_size_++] = std::byte{0x80};
    if (block_size_ > 56) {
        std::fill(block_.begin() + block_size_, block_.end(), std::byte{0});
        transform(block_);
        block_size_ = 0;
    }
    std::fill(block_.begin() + block_size_, block_.begin() + 56, std::byte{0});
    for (std::size_t index = 0; index < 8; ++index) {
        block_[56 + index] = static_cast<std::byte>(
            (bit_length >> ((7U - index) * 8U)) & 0xffU);
    }
    transform(block_);

    for (std::size_t word = 0; word < state_.size(); ++word) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            digest_output[word * 4U + byte] = static_cast<std::byte>(
                (state_[word] >> ((3U - byte) * 8U)) & 0xffU);
        }
    }
}

} // namespace marc::core

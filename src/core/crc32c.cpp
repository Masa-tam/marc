#include "core/crc32c.hpp"

#include "core/endian.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {
namespace {

constexpr std::uint32_t reflected_polynomial = 0x82f63b78U;
constexpr std::uint32_t initial_register = 0xffffffffU;
constexpr std::uint32_t final_xor = 0xffffffffU;

} // namespace

void Crc32c::reset() noexcept {
    register_ = initial_register;
}

bool Crc32c::update(const std::span<const std::byte> bytes) noexcept {
    for (const std::byte byte : bytes) {
        register_ ^= std::to_integer<std::uint8_t>(byte);
        for (unsigned int bit = 0; bit < 8; ++bit) {
            const std::uint32_t low_bit_mask =
                std::uint32_t{0} - (register_ & std::uint32_t{1});
            register_ =
                (register_ >> 1U) ^ (reflected_polynomial & low_bit_mask);
        }
    }
    return true;
}

bool Crc32c::finalize(
    const std::span<std::byte> digest_output) noexcept {
    if (digest_output.size() != crc32c_digest_size) {
        return false;
    }
    return store_le(digest_output, 0, register_ ^ final_xor);
}

std::size_t Crc32c::digest_size() const noexcept {
    return crc32c_digest_size;
}

HashAlgorithmId Crc32c::algorithm_id() const noexcept {
    return crc32c_algorithm_id;
}

} // namespace marc::core

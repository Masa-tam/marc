#ifndef MARC_CORE_SHA256_HPP
#define MARC_CORE_SHA256_HPP

#include "core/hash.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

inline constexpr HashAlgorithmId sha256_algorithm_id = 2;
inline constexpr std::size_t sha256_digest_size = 32;

class Sha256 final : public IHashAlgorithm {
public:
    void reset() noexcept override;

    [[nodiscard]] bool update(
        std::span<const std::byte> bytes) noexcept override;
    [[nodiscard]] bool finalize(
        std::span<std::byte> digest_output) noexcept override;
    [[nodiscard]] std::size_t digest_size() const noexcept override;
    [[nodiscard]] HashAlgorithmId algorithm_id() const noexcept override;

private:
    void transform(std::span<const std::byte> block) noexcept;
    void finish(std::span<std::byte> digest_output) noexcept;

    std::array<std::uint32_t, 8> state_{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
    };
    std::array<std::byte, 64> block_{};
    std::uint64_t total_bytes_{};
    std::size_t block_size_{};
};

} // namespace marc::core

#endif

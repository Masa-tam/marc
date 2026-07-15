#ifndef MARC_CORE_CRC32C_HPP
#define MARC_CORE_CRC32C_HPP

#include "core/hash.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

inline constexpr HashAlgorithmId crc32c_algorithm_id = 1;
inline constexpr std::size_t crc32c_digest_size = 4;

class Crc32c final : public IHashAlgorithm {
public:
    void reset() noexcept override;

    [[nodiscard]] bool update(
        std::span<const std::byte> bytes) noexcept override;
    [[nodiscard]] bool finalize(
        std::span<std::byte> digest_output) noexcept override;
    [[nodiscard]] std::size_t digest_size() const noexcept override;
    [[nodiscard]] HashAlgorithmId algorithm_id() const noexcept override;

private:
    std::uint32_t register_{0xffffffffU};
};

} // namespace marc::core

#endif

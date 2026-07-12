#ifndef MARC_CORE_HASH_HPP
#define MARC_CORE_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

using HashAlgorithmId = std::uint32_t;

class IHashAlgorithm {
public:
    virtual ~IHashAlgorithm() = default;

    virtual void reset() noexcept = 0;
    [[nodiscard]] virtual bool update(
        std::span<const std::byte> bytes) noexcept = 0;
    [[nodiscard]] virtual bool finalize(
        std::span<std::byte> digest_output) noexcept = 0;
    [[nodiscard]] virtual std::size_t digest_size() const noexcept = 0;
    [[nodiscard]] virtual HashAlgorithmId algorithm_id() const noexcept = 0;
};

} // namespace marc::core

#endif

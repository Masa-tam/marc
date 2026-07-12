#ifndef MARC_CORE_HASH_TAP_HPP
#define MARC_CORE_HASH_TAP_HPP

#include "core/hash.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::core {

enum class HashTapStatus : std::uint8_t {
    ok,
    invalid_argument,
    algorithm_failure,
    already_finalized,
    error_state,
    size_overflow,
};

enum class HashTapState : std::uint8_t {
    running,
    finalized,
    error,
};

class HashTap final {
public:
    explicit HashTap(IHashAlgorithm& algorithm) noexcept;

    void reset() noexcept;

    [[nodiscard]] HashTapStatus commit(
        std::span<const std::byte> available_bytes,
        std::size_t committed_size) noexcept;

    [[nodiscard]] HashTapStatus finalize(
        std::span<std::byte> digest_output) noexcept;

    [[nodiscard]] HashTapState state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t total_committed() const noexcept {
        return total_committed_;
    }
    [[nodiscard]] HashAlgorithmId algorithm_id() const noexcept {
        return algorithm_.algorithm_id();
    }
    [[nodiscard]] std::size_t digest_size() const noexcept {
        return algorithm_.digest_size();
    }

private:
    IHashAlgorithm& algorithm_;
    std::uint64_t total_committed_{};
    HashTapState state_{HashTapState::running};
};

} // namespace marc::core

#endif

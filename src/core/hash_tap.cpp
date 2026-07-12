#include "core/hash_tap.hpp"

#include "core/checked_math.hpp"

#include <limits>

namespace marc::core {

HashTap::HashTap(IHashAlgorithm& algorithm) noexcept
    : algorithm_{algorithm} {
    algorithm_.reset();
}

void HashTap::reset() noexcept {
    algorithm_.reset();
    total_committed_ = 0;
    state_ = HashTapState::running;
}

HashTapStatus HashTap::commit(
    const std::span<const std::byte> available_bytes,
    const std::size_t committed_size) noexcept {
    if (state_ == HashTapState::finalized) {
        return HashTapStatus::already_finalized;
    }
    if (state_ == HashTapState::error) {
        return HashTapStatus::error_state;
    }
    if (committed_size > available_bytes.size()) {
        return HashTapStatus::invalid_argument;
    }
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        if (committed_size > std::numeric_limits<std::uint64_t>::max()) {
            state_ = HashTapState::error;
            return HashTapStatus::size_overflow;
        }
    }

    std::uint64_t new_total{};
    if (!checked_add(total_committed_,
                     static_cast<std::uint64_t>(committed_size),
                     new_total)) {
        state_ = HashTapState::error;
        return HashTapStatus::size_overflow;
    }
    if (!algorithm_.update(available_bytes.first(committed_size))) {
        state_ = HashTapState::error;
        return HashTapStatus::algorithm_failure;
    }
    total_committed_ = new_total;
    return HashTapStatus::ok;
}

HashTapStatus HashTap::finalize(
    const std::span<std::byte> digest_output) noexcept {
    if (state_ == HashTapState::finalized) {
        return HashTapStatus::already_finalized;
    }
    if (state_ == HashTapState::error) {
        return HashTapStatus::error_state;
    }
    if (digest_output.size() != algorithm_.digest_size()) {
        return HashTapStatus::invalid_argument;
    }
    if (!algorithm_.finalize(digest_output)) {
        state_ = HashTapState::error;
        return HashTapStatus::algorithm_failure;
    }
    state_ = HashTapState::finalized;
    return HashTapStatus::ok;
}

} // namespace marc::core

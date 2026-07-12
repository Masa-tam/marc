#ifndef MARC_ENTROPY_RANS_NORMALIZER_HPP
#define MARC_ENTROPY_RANS_NORMALIZER_HPP

#include "core/limits.hpp"
#include "entropy/rans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class RansNormalizationError : std::uint8_t {
    none,
    empty_input,
    block_too_large,
    limit_exceeded,
    internal_error,
};

[[nodiscard]] RansNormalizationError normalize_rans_frequencies(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::array<std::uint16_t, 256>& frequencies) noexcept;

} // namespace marc::entropy::internal

#endif

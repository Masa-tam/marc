#ifndef MARC_ENTROPY_RANS_DECODER_HPP
#define MARC_ENTROPY_RANS_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class RansDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    output_too_small,
    invalid_state,
    invalid_table,
    truncated_payload,
    trailing_payload,
    invalid_terminal_state,
    arithmetic_overflow,
    internal_error,
};

struct RansDecodeResult {
    std::size_t output_size{};
    std::size_t payload_consumed{};
    RansDecodeError error{RansDecodeError::none};
};

[[nodiscard]] RansDecodeResult validate_rans_block(
    const RansDescriptor& descriptor,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] RansDecodeResult decode_rans_block(
    const RansDescriptor& descriptor,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif

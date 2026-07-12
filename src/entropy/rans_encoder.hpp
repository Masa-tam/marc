#ifndef MARC_ENTROPY_RANS_ENCODER_HPP
#define MARC_ENTROPY_RANS_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class RansEncodeError : std::uint8_t {
    none,
    empty_input,
    block_too_large,
    payload_output_too_small,
    limit_exceeded,
    normalization_error,
    arithmetic_overflow,
    internal_error,
};

struct RansEncodeResult {
    std::size_t payload_size{};
    RansEncodeError error{RansEncodeError::none};
};

[[nodiscard]] RansEncodeResult plan_rans_block(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    RansDescriptor& descriptor) noexcept;

[[nodiscard]] RansEncodeResult encode_rans_block(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    RansDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

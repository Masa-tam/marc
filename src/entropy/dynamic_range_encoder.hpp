#ifndef MARC_ENTROPY_DYNAMIC_RANGE_ENCODER_HPP
#define MARC_ENTROPY_DYNAMIC_RANGE_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/dynamic_range_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class DynamicRangeEncodeError : std::uint8_t {
    none,
    empty_input,
    frame_too_large,
    payload_output_too_small,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct DynamicRangeEncodeResult {
    std::size_t payload_size{};
    DynamicRangeEncodeError error{DynamicRangeEncodeError::none};
};

[[nodiscard]] DynamicRangeEncodeResult plan_dynamic_range_frame(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    DynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] DynamicRangeEncodeResult encode_dynamic_range_frame(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    DynamicRangeDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

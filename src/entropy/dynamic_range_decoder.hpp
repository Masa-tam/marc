#ifndef MARC_ENTROPY_DYNAMIC_RANGE_DECODER_HPP
#define MARC_ENTROPY_DYNAMIC_RANGE_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/dynamic_range_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class DynamicRangeDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    output_too_small,
    truncated_payload,
    invalid_interval,
    trailing_payload,
    invalid_model,
    internal_error,
};

struct DynamicRangeDecodeResult {
    std::size_t output_size{};
    std::size_t payload_consumed{};
    DynamicRangeDecodeError error{DynamicRangeDecodeError::none};
};

[[nodiscard]] DynamicRangeDecodeResult validate_dynamic_range_frame(
    const DynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] DynamicRangeDecodeResult decode_dynamic_range_frame(
    const DynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif

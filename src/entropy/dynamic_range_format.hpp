#ifndef MARC_ENTROPY_DYNAMIC_RANGE_FORMAT_HPP
#define MARC_ENTROPY_DYNAMIC_RANGE_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t dynamic_range_descriptor_size = 16;
inline constexpr std::uint32_t dynamic_range_max_frame_size = UINT32_C(1) << 24;
inline constexpr std::uint32_t dynamic_range_model_total_limit = UINT32_C(1) << 15;
inline constexpr std::uint32_t dynamic_range_min_payload_size = 5;

struct DynamicRangeDescriptor {
    std::uint32_t symbol_count{};
    std::uint32_t payload_size{};
    std::uint8_t flags{};
};

enum class DynamicRangeFormatError : std::uint8_t {
    none,
    invalid_symbol_count,
    invalid_payload_size,
    unknown_flags,
    nonzero_reserved,
    contradictory_size,
    limit_exceeded,
};

[[nodiscard]] DynamicRangeFormatError validate_dynamic_range_descriptor(
    const DynamicRangeDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] DynamicRangeFormatError parse_dynamic_range_descriptor(
    std::span<const std::byte, dynamic_range_descriptor_size> input,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    DynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] DynamicRangeFormatError serialize_dynamic_range_descriptor(
    const DynamicRangeDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte, dynamic_range_descriptor_size> output) noexcept;

} // namespace marc::entropy::internal

#endif

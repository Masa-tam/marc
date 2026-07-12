#ifndef MARC_ENTROPY_RANS_FORMAT_HPP
#define MARC_ENTROPY_RANS_FORMAT_HPP

#include "core/limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint8_t rans_table_log = 12;
inline constexpr std::uint32_t rans_total_frequency = UINT32_C(1) << rans_table_log;
inline constexpr std::uint64_t rans_lower_bound = UINT64_C(1) << 31;
inline constexpr std::uint32_t rans_max_block_size = UINT32_C(1) << 24;
inline constexpr std::uint32_t rans_min_payload_size = 8;
inline constexpr std::size_t rans_descriptor_size = 528;

struct RansDescriptor {
    std::uint32_t symbol_count{};
    std::uint32_t payload_size{};
    std::uint8_t table_log{rans_table_log};
    std::uint8_t flags{};
    std::array<std::uint16_t, 256> frequencies{};
};

enum class RansFormatError : std::uint8_t {
    none,
    invalid_symbol_count,
    invalid_payload_size,
    invalid_table_log,
    unknown_flags,
    nonzero_reserved,
    invalid_frequency_table,
    contradictory_size,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] RansFormatError validate_rans_descriptor(
    const RansDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] RansFormatError parse_rans_descriptor(
    std::span<const std::byte, rans_descriptor_size> input,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    RansDescriptor& descriptor) noexcept;

[[nodiscard]] RansFormatError serialize_rans_descriptor(
    const RansDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::span<std::byte, rans_descriptor_size> output) noexcept;

} // namespace marc::entropy::internal

#endif

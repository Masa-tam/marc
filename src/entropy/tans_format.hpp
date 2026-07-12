#ifndef MARC_ENTROPY_TANS_FORMAT_HPP
#define MARC_ENTROPY_TANS_FORMAT_HPP

#include "core/limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::uint8_t tans_table_log = 12;
inline constexpr std::uint32_t tans_table_size = UINT32_C(1) << tans_table_log;
inline constexpr std::uint32_t tans_spread_step = 2563;
inline constexpr std::uint32_t tans_max_block_size = UINT32_C(1) << 24;
inline constexpr std::uint32_t tans_min_payload_size = 2;
inline constexpr std::size_t tans_descriptor_size = 528;

struct TansDescriptor {
    std::uint32_t symbol_count{};
    std::uint32_t payload_size{};
    std::uint8_t table_log{tans_table_log};
    std::uint8_t final_valid_bits{};
    std::uint8_t flags{};
    std::array<std::uint16_t, 256> frequencies{};
};

enum class TansFormatError : std::uint8_t {
    none,
    invalid_symbol_count,
    invalid_payload_size,
    invalid_table_log,
    invalid_valid_bits,
    unknown_flags,
    nonzero_reserved,
    invalid_frequency_table,
    contradictory_size,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] TansFormatError validate_tans_descriptor(
    const TansDescriptor& descriptor, std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] TansFormatError parse_tans_descriptor(
    std::span<const std::byte, tans_descriptor_size> input,
    std::uint32_t expected_symbol_count, std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits, TansDescriptor& descriptor) noexcept;

[[nodiscard]] TansFormatError serialize_tans_descriptor(
    const TansDescriptor& descriptor, std::uint32_t expected_symbol_count,
    std::uint32_t expected_payload_size, const core::DecoderLimits& limits,
    std::span<std::byte, tans_descriptor_size> output) noexcept;

} // namespace marc::entropy::internal

#endif

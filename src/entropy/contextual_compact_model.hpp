#ifndef MARC_ENTROPY_CONTEXTUAL_COMPACT_MODEL_HPP
#define MARC_ENTROPY_CONTEXTUAL_COMPACT_MODEL_HPP

#include "context/lzss_field_context_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_compact_model_min_records_size = 3;
inline constexpr std::size_t contextual_compact_model_max_records_size = 9005;
inline constexpr std::uint32_t contextual_compact_model_total_frequency =
    UINT32_C(4096);

using ContextualCompactFrequencies = std::array<
    std::uint16_t,
    context::internal::lzss_field_context_frequency_entries>;

enum class ContextualCompactModelError : std::uint8_t {
    none,
    invalid_active_context_mask,
    truncated_records,
    invalid_mode,
    invalid_frequency_table,
    noncanonical_representation,
    trailing_data,
    arithmetic_overflow,
    output_too_small,
};

struct ContextualCompactModelAnalysis {
    std::uint32_t active_mask{};
    std::size_t records_size{};
    ContextualCompactModelError error{ContextualCompactModelError::none};
};

[[nodiscard]] ContextualCompactModelAnalysis
analyze_contextual_compact_model(
    const ContextualCompactFrequencies& frequencies) noexcept;

[[nodiscard]] ContextualCompactModelError parse_contextual_compact_model(
    std::span<const std::byte> input,
    std::uint32_t active_mask,
    ContextualCompactFrequencies& frequencies) noexcept;

[[nodiscard]] ContextualCompactModelError serialize_contextual_compact_model(
    const ContextualCompactFrequencies& frequencies,
    std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;

} // namespace marc::entropy::internal

#endif

// Private 4 MiB operation core; no public/frame admission.
#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_4M_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_4M_CONTEXT_LAYOUT_HPP

#include "context/lzss_reduced_literal_context_layout.hpp"

namespace marc::context::internal {

// Private 2/10 + 1/11 + 3/2. This predicate is not stream admission/preflight.
inline constexpr std::uint16_t lzss_position_distance_4m_context_count = 46;
inline constexpr std::size_t lzss_position_distance_4m_frequency_entries = 2588;
inline constexpr auto lzss_position_distance_4m_alphabets = [] {
    std::array<std::uint16_t, lzss_position_distance_4m_context_count> values{};
    for (std::size_t i = 0; i < 24; ++i)
        values[i] = lzss_reduced_literal_alphabets[i];
    for (std::size_t i = 15; i < 24; ++i) values[i] = 23;
    for (std::size_t i = 24; i < values.size(); ++i) values[i] = 2;
    return values;
}();
inline constexpr auto lzss_position_distance_4m_offsets = [] {
    std::array<std::size_t, lzss_position_distance_4m_context_count + 1> values{};
    for (std::size_t i = 0; i < lzss_position_distance_4m_context_count; ++i)
        values[i + 1] = values[i] + lzss_position_distance_4m_alphabets[i];
    return values;
}();
static_assert(lzss_position_distance_4m_offsets[24] == 2544);
static_assert(lzss_position_distance_4m_offsets.back() == 2588);

[[nodiscard]] constexpr bool is_lzss_position_distance_4m_identity(
    const std::uint16_t dictionary_algorithm, const std::uint16_t dictionary_variant,
    const std::uint16_t context_algorithm, const std::uint16_t context_variant,
    const std::uint16_t entropy_algorithm, const std::uint16_t entropy_variant,
    const std::uint16_t context_count) noexcept {
    return dictionary_algorithm == 2 && dictionary_variant == 10
        && context_algorithm == 1 && context_variant == 11
        && entropy_algorithm == 3 && entropy_variant == 2
        && context_count == lzss_position_distance_4m_context_count;
}

// Only ordinary field symbols are accepted here. IDs 24..45 belong to the
// backend's grouped distance-extra operation, never arbitrary token symbols.
[[nodiscard]] constexpr LzssFieldContextError validate_lzss_position_distance_4m_symbol(
    const ModeledOperation& operation) noexcept {
    if (operation.kind != ModeledOperationKind::symbol)
        return LzssFieldContextError::unexpected_operation_kind;
    if (operation.context_id >= 24) return LzssFieldContextError::unexpected_context;
    const auto alphabet = lzss_position_distance_4m_alphabets[operation.context_id];
    if (operation.alphabet_size != alphabet) return LzssFieldContextError::unexpected_alphabet;
    if (operation.value >= alphabet) return LzssFieldContextError::invalid_symbol;
    if (operation.bit_count != 0) return LzssFieldContextError::nonzero_unused_field;
    return LzssFieldContextError::none;
}

} // namespace marc::context::internal
#endif

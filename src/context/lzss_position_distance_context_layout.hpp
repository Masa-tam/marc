#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_CONTEXT_LAYOUT_HPP

#include "context/lzss_reduced_literal_context_layout.hpp"

namespace marc::context::internal {

// Private 2/8 + 1/9 + 3/2. This predicate is not stream admission/preflight.
inline constexpr std::uint16_t lzss_position_distance_context_count = 40;
inline constexpr std::size_t lzss_position_distance_frequency_entries = 2522;
inline constexpr auto lzss_position_distance_alphabets = [] {
    std::array<std::uint16_t, lzss_position_distance_context_count> values{};
    for (std::size_t i = 0; i < 24; ++i)
        values[i] = lzss_reduced_literal_alphabets[i];
    for (std::size_t i = 24; i < values.size(); ++i) values[i] = 2;
    return values;
}();
inline constexpr auto lzss_position_distance_offsets = [] {
    std::array<std::size_t, lzss_position_distance_context_count + 1> values{};
    for (std::size_t i = 0; i < lzss_position_distance_context_count; ++i)
        values[i + 1] = values[i] + lzss_position_distance_alphabets[i];
    return values;
}();
static_assert(lzss_position_distance_offsets[24] == 2490);
static_assert(lzss_position_distance_offsets.back() == 2522);

[[nodiscard]] constexpr bool is_lzss_position_distance_identity(
    const std::uint16_t dictionary_algorithm, const std::uint16_t dictionary_variant,
    const std::uint16_t context_algorithm, const std::uint16_t context_variant,
    const std::uint16_t entropy_algorithm, const std::uint16_t entropy_variant,
    const std::uint16_t context_count) noexcept {
    return dictionary_algorithm == 2 && dictionary_variant == 8
        && context_algorithm == 1 && context_variant == 9
        && entropy_algorithm == 3 && entropy_variant == 2
        && context_count == lzss_position_distance_context_count;
}

// Only ordinary field symbols are accepted here. IDs 24..39 belong to the
// backend's grouped distance-extra operation, never arbitrary token symbols.
[[nodiscard]] constexpr LzssFieldContextError validate_lzss_position_distance_symbol(
    const ModeledOperation& operation) noexcept {
    return validate_lzss_reduced_literal_symbol(operation);
}

} // namespace marc::context::internal
#endif

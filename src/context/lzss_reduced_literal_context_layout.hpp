#ifndef MARC_CONTEXT_LZSS_REDUCED_LITERAL_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_REDUCED_LITERAL_CONTEXT_LAYOUT_HPP

#include "context/lzss_field_context.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {

// Private 2/8 + 1/8 + 3/2 representation. No public selector uses this layout.
inline constexpr std::uint16_t lzss_reduced_literal_context_count = 24;
inline constexpr std::size_t lzss_reduced_literal_frequency_entries = 2490;
inline constexpr auto lzss_reduced_literal_alphabets = [] {
    std::array<std::uint16_t, lzss_reduced_literal_context_count> values{};
    for (std::size_t i = 0; i < 3; ++i) values[i] = 2;
    for (std::size_t i = 3; i < 12; ++i) values[i] = 256;
    for (std::size_t i = 12; i < 15; ++i) values[i] = 9;
    for (std::size_t i = 15; i < 24; ++i) values[i] = 17;
    return values;
}();
inline constexpr auto lzss_reduced_literal_offsets = [] {
    std::array<std::size_t, lzss_reduced_literal_context_count + 1> values{};
    for (std::size_t i = 0; i < lzss_reduced_literal_context_count; ++i)
        values[i + 1] = values[i] + lzss_reduced_literal_alphabets[i];
    return values;
}();
static_assert(lzss_reduced_literal_offsets[3] == 6);
static_assert(lzss_reduced_literal_offsets[12] == 2310);
static_assert(lzss_reduced_literal_offsets[15] == 2337);
static_assert(lzss_reduced_literal_offsets.back() == lzss_reduced_literal_frequency_entries);

// A narrow identity/count predicate, not a stream or frame preflight.
[[nodiscard]] constexpr bool is_lzss_reduced_literal_identity(
    const std::uint16_t dictionary_algorithm, const std::uint16_t dictionary_variant,
    const std::uint16_t context_algorithm, const std::uint16_t context_variant,
    const std::uint16_t entropy_algorithm, const std::uint16_t entropy_variant,
    const std::uint16_t context_count) noexcept {
    return dictionary_algorithm == 2 && dictionary_variant == 8
        && context_algorithm == 1 && context_variant == 8
        && entropy_algorithm == 3 && entropy_variant == 2
        && context_count == lzss_reduced_literal_context_count;
}

// Validate one symbol's storage shape before indexing the model bank.
// Token-state order, bypass grammar and frame limits require separate validation.
[[nodiscard]] constexpr LzssFieldContextError validate_lzss_reduced_literal_symbol(
    const ModeledOperation& operation) noexcept {
    if (operation.kind != ModeledOperationKind::symbol)
        return LzssFieldContextError::unexpected_operation_kind;
    if (operation.context_id >= lzss_reduced_literal_context_count)
        return LzssFieldContextError::unexpected_context;
    const auto alphabet = lzss_reduced_literal_alphabets[operation.context_id];
    if (operation.alphabet_size != alphabet)
        return LzssFieldContextError::unexpected_alphabet;
    if (operation.value >= alphabet) return LzssFieldContextError::invalid_symbol;
    if (operation.bit_count != 0) return LzssFieldContextError::nonzero_unused_field;
    return LzssFieldContextError::none;
}

} // namespace marc::context::internal
#endif

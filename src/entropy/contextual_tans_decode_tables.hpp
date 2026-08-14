#ifndef MARC_ENTROPY_CONTEXTUAL_TANS_DECODE_TABLES_HPP
#define MARC_ENTROPY_CONTEXTUAL_TANS_DECODE_TABLES_HPP

#include "core/limits.hpp"
#include "entropy/contextual_tans_format.hpp"
#include "entropy/tans_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_tans_bypass_table_index =
    contextual_tans_context_count;

struct ContextualTansDecodeTables {
    std::span<TansDecodeEntry> entries{};
    std::array<bool, contextual_tans_context_count> active_contexts{};
};

enum class ContextualTansDecodeTableError : std::uint8_t {
    none,
    invalid_descriptor,
    output_too_small,
    invalid_transition_table,
};

struct ContextualTansDecodeTableResult {
    std::size_t required_entries{
        static_cast<std::size_t>(contextual_tans_decode_table_entries)};
    std::size_t active_context_count{};
    ContextualTansFormatError format_error{ContextualTansFormatError::none};
    TansTableError table_error{TansTableError::none};
    ContextualTansDecodeTableError error{
        ContextualTansDecodeTableError::none};
};

[[nodiscard]] ContextualTansDecodeTableResult
build_contextual_tans_decode_tables(
    const ContextualTansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    std::span<TansDecodeEntry> output,
    ContextualTansDecodeTables& tables,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::entropy::internal

#endif

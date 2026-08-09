#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_DECODE_TABLES_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_DECODE_TABLES_HPP

#include "core/limits.hpp"
#include "entropy/contextual_rans_format.hpp"
#include "entropy/rans_decode_table.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct ContextualRansDecodeTables {
    std::span<RansDecodeEntry> entries{};
    std::array<bool, contextual_rans_context_count> active_contexts{};
};

enum class ContextualRansDecodeTableError : std::uint8_t {
    none,
    invalid_descriptor,
    output_too_small,
};

struct ContextualRansDecodeTableResult {
    std::size_t required_entries{contextual_rans_decode_table_entries};
    std::size_t active_context_count{};
    ContextualRansFormatError format_error{ContextualRansFormatError::none};
    ContextualRansDecodeTableError error{ContextualRansDecodeTableError::none};
};

[[nodiscard]] ContextualRansDecodeTableResult
build_contextual_rans_decode_tables(
    const ContextualRansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    std::span<RansDecodeEntry> output,
    ContextualRansDecodeTables& tables) noexcept;

} // namespace marc::entropy::internal

#endif

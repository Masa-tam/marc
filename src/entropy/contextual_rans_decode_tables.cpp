#include "entropy/contextual_rans_decode_tables.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

ContextualRansDecodeTableResult build_contextual_rans_decode_tables(
    const ContextualRansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    const std::span<RansDecodeEntry> output,
    ContextualRansDecodeTables& tables,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualRansDecodeTableResult result{};
    std::size_t descriptor_size{};
    result.format_error = validate_contextual_rans_descriptor(
        descriptor, descriptor.decision_count, descriptor.payload_size,
        limits, descriptor_size, variant);
    if (result.format_error != ContextualRansFormatError::none) {
        result.error = ContextualRansDecodeTableError::invalid_descriptor;
        return result;
    }
    return build_contextual_rans_decode_tables_from_model(
        descriptor, output, tables, variant);
}

ContextualRansDecodeTableResult
build_contextual_rans_decode_tables_from_model(
    const ContextualRansDescriptor& descriptor,
    const std::span<RansDecodeEntry> output,
    ContextualRansDecodeTables& tables,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualRansDecodeTableResult result{};
    result.format_error = validate_contextual_rans_model(
        descriptor, descriptor.decision_count, descriptor.payload_size,
        variant);
    if (result.format_error != ContextualRansFormatError::none) {
        result.error = ContextualRansDecodeTableError::invalid_descriptor;
        return result;
    }
    if (output.size() < contextual_rans_decode_table_entries) {
        result.error = ContextualRansDecodeTableError::output_too_small;
        return result;
    }

    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.format_error =
            ContextualRansFormatError::unsupported_context_variant;
        result.error = ContextualRansDecodeTableError::invalid_descriptor;
        return result;
    }
    const auto& layout = selected.layout;

    const auto frequencies = descriptor.frequencies;
    std::array<bool, contextual_rans_context_count> active{};
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        const auto frequency_begin =
            (*layout.offsets)[context_id];
        const auto frequency_end = (*layout.offsets)[context_id + 1];
        active[context_id] = std::any_of(
            frequencies.begin() + frequency_begin,
            frequencies.begin() + frequency_end,
            [](const std::uint16_t frequency) { return frequency != 0; });
        if (active[context_id]) ++result.active_context_count;
    }

    const auto used_output = output.first(contextual_rans_decode_table_entries);
    std::fill(used_output.begin(), used_output.end(), RansDecodeEntry{});
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        if (!active[context_id]) continue;
        const auto frequency_begin =
            (*layout.offsets)[context_id];
        const auto alphabet = (*layout.alphabets)[context_id];
        const auto table_begin =
            context_id * contextual_rans_total_frequency;
        std::uint32_t cumulative{};
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            const auto frequency = frequencies[frequency_begin + symbol];
            if (frequency == 0) continue;
            const RansDecodeEntry entry{
                static_cast<std::uint16_t>(cumulative), frequency,
                static_cast<std::uint8_t>(symbol)};
            const auto end = cumulative + frequency;
            for (auto slot = cumulative; slot < end; ++slot) {
                used_output[table_begin + slot] = entry;
            }
            cumulative = end;
        }
    }

    ContextualRansDecodeTables built{};
    built.entries = used_output;
    built.active_contexts = active;
    tables = built;
    return result;
}

} // namespace marc::entropy::internal

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
    ContextualRansDecodeTables& tables) noexcept {
    ContextualRansDecodeTableResult result{};
    result.format_error = validate_contextual_rans_descriptor(
        descriptor, descriptor.decision_count, descriptor.payload_size,
        limits);
    if (result.format_error != ContextualRansFormatError::none) {
        result.error = ContextualRansDecodeTableError::invalid_descriptor;
        return result;
    }
    if (output.size() < contextual_rans_decode_table_entries) {
        result.error = ContextualRansDecodeTableError::output_too_small;
        return result;
    }

    const auto frequencies = descriptor.frequencies;
    std::array<bool, contextual_rans_context_count> active{};
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        const auto frequency_begin =
            marc::context::internal::lzss_field_context_offsets[context_id];
        const auto frequency_end =
            marc::context::internal::lzss_field_context_offsets[
                context_id + 1];
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
            marc::context::internal::lzss_field_context_offsets[context_id];
        const auto alphabet =
            marc::context::internal::lzss_field_context_alphabets[context_id];
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

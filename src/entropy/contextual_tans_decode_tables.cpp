#include "entropy/contextual_tans_decode_tables.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] TansDescriptor make_table_descriptor(
    const ContextualCompactFrequencies& frequencies,
    const std::size_t context_id) noexcept {
    TansDescriptor descriptor{};
    const auto begin =
        context::internal::lzss_field_context_offsets[context_id];
    const auto alphabet =
        context::internal::lzss_field_context_alphabets[context_id];
    std::copy_n(
        frequencies.begin() + begin, alphabet,
        descriptor.frequencies.begin());
    return descriptor;
}

[[nodiscard]] TansDescriptor make_bypass_descriptor() noexcept {
    TansDescriptor descriptor{};
    descriptor.frequencies[0] =
        static_cast<std::uint16_t>(contextual_tans_total_frequency / 2);
    descriptor.frequencies[1] =
        static_cast<std::uint16_t>(contextual_tans_total_frequency / 2);
    return descriptor;
}

} // namespace

ContextualTansDecodeTableResult build_contextual_tans_decode_tables(
    const ContextualTansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    const std::span<TansDecodeEntry> output,
    ContextualTansDecodeTables& tables) noexcept {
    ContextualTansDecodeTableResult result{};
    std::size_t serialized_size{};
    result.format_error = validate_contextual_tans_descriptor(
        descriptor, descriptor.decision_count, descriptor.payload_size,
        limits, serialized_size);
    if (result.format_error != ContextualTansFormatError::none) {
        result.error = ContextualTansDecodeTableError::invalid_descriptor;
        return result;
    }
    if (output.size() < contextual_tans_decode_table_entries) {
        result.error = ContextualTansDecodeTableError::output_too_small;
        return result;
    }

    // Snapshot the compact model before touching caller-owned output. This
    // also makes descriptor/output aliasing harmless.
    const auto frequencies = descriptor.frequencies;
    std::array<bool, contextual_tans_context_count> active{};
    for (std::size_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        const auto begin =
            context::internal::lzss_field_context_offsets[context_id];
        const auto end =
            context::internal::lzss_field_context_offsets[context_id + 1];
        active[context_id] = std::any_of(
            frequencies.begin() + begin, frequencies.begin() + end,
            [](const std::uint16_t frequency) { return frequency != 0; });
        if (active[context_id]) ++result.active_context_count;
    }

    // Validate every transition table before publishing any output. A second
    // deterministic pass below cannot fail for the same immutable snapshot.
    TansTables scratch{};
    for (std::size_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        if (!active[context_id]) continue;
        result.table_error = build_tans_tables(
            make_table_descriptor(frequencies, context_id), scratch);
        if (result.table_error != TansTableError::none) {
            result.error =
                ContextualTansDecodeTableError::invalid_transition_table;
            return result;
        }
    }
    const auto bypass_descriptor = make_bypass_descriptor();
    result.table_error = build_tans_tables(bypass_descriptor, scratch);
    if (result.table_error != TansTableError::none) {
        result.error =
            ContextualTansDecodeTableError::invalid_transition_table;
        return result;
    }

    const auto used = output.first(
        static_cast<std::size_t>(contextual_tans_decode_table_entries));
    std::fill(used.begin(), used.end(), TansDecodeEntry{});
    for (std::size_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        if (!active[context_id]) continue;
        const auto ignored = build_tans_tables(
            make_table_descriptor(frequencies, context_id), scratch);
        (void)ignored;
        std::copy(
            scratch.decode.begin(), scratch.decode.end(),
            used.begin() + context_id * contextual_tans_total_frequency);
    }
    const auto ignored = build_tans_tables(bypass_descriptor, scratch);
    (void)ignored;
    std::copy(
        scratch.decode.begin(), scratch.decode.end(),
        used.begin()
            + contextual_tans_bypass_table_index
                * contextual_tans_total_frequency);

    ContextualTansDecodeTables built{};
    built.entries = used;
    built.active_contexts = active;
    tables = built;
    result.table_error = TansTableError::none;
    return result;
}

} // namespace marc::entropy::internal

#include "frame/lzss_contextual_adaptive_huffman_frame_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace marc::frame::internal {
namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssContextualAdaptiveHuffmanFrameDecodeResult
decode_lzss_contextual_adaptive_huffman_frame(
    const std::span<const std::byte> serialized_frame,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> private_raw_output) noexcept {
    using E = LzssContextualAdaptiveHuffmanFrameDecodeError;
    LzssContextualAdaptiveHuffmanFrameDecodeResult result{};
    LzssContextualAdaptiveHuffmanFrameLayout layout{};
    result.preflight = preflight_lzss_contextual_adaptive_huffman_frame(
        serialized_frame, context, layout);
    if (result.preflight.error
        != LzssContextualAdaptiveHuffmanFramePreflightError::none) {
        result.error = E::preflight_error;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        context.stream.dictionary_variant,
        context.stream.context_algorithm, context.stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = E::preflight_error;
        result.preflight.error =
            LzssContextualAdaptiveHuffmanFramePreflightError::header_error;
        result.preflight.header_error =
            LzssContextualAdaptiveHuffmanFrameHeaderError::
                invalid_stream_header;
        return result;
    }
    if (!std::in_range<std::size_t>(layout.header.token_count)
        || !std::in_range<std::size_t>(layout.header.uncompressed_size)
        || !std::in_range<std::size_t>(layout.header.descriptor_size)
        || !std::in_range<std::size_t>(layout.header.payload_size)) {
        result.error = E::output_size_unsupported;
        return result;
    }

    result.required_node_entries =
        2 * selected.layout.frequency_entries
        + context::internal::lzss_field_context_count;
    result.required_symbol_entries = selected.layout.frequency_entries;
    result.required_token_count =
        static_cast<std::size_t>(layout.header.token_count);
    result.required_raw_size =
        static_cast<std::size_t>(layout.header.uncompressed_size);
    if (private_nodes.size() < result.required_node_entries) {
        result.error = E::node_workspace_too_small;
        return result;
    }
    if (private_symbols.size() < result.required_symbol_entries) {
        result.error = E::symbol_workspace_too_small;
        return result;
    }
    if (private_tokens.size() < result.required_token_count) {
        result.error = E::token_output_too_small;
        return result;
    }
    if (private_raw_output.size() < result.required_raw_size) {
        result.error = E::raw_output_too_small;
        return result;
    }

    const auto nodes = private_nodes.first(result.required_node_entries);
    const auto symbols = private_symbols.first(result.required_symbol_entries);
    const auto tokens = private_tokens.first(result.required_token_count);
    const auto raw = private_raw_output.first(result.required_raw_size);
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            nodes.size(), sizeof(entropy::internal::AdaptiveHuffmanNode),
            node_bytes)
        || !core::checked_multiply(
            symbols.size(), sizeof(std::uint16_t), symbol_bytes)
        || !core::checked_multiply(
            tokens.size(), sizeof(dictionary::internal::LzssTypedToken),
            token_bytes)) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    const std::array overlaps{
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        nodes.data(), node_bytes),
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        symbols.data(), symbol_bytes),
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        tokens.data(), token_bytes),
        regions_overlap(serialized_frame.data(), layout.serialized_size,
                        raw.data(), raw.size()),
        regions_overlap(nodes.data(), node_bytes, symbols.data(), symbol_bytes),
        regions_overlap(nodes.data(), node_bytes, tokens.data(), token_bytes),
        regions_overlap(nodes.data(), node_bytes, raw.data(), raw.size()),
        regions_overlap(symbols.data(), symbol_bytes, tokens.data(),
                        token_bytes),
        regions_overlap(symbols.data(), symbol_bytes, raw.data(), raw.size()),
        regions_overlap(tokens.data(), token_bytes, raw.data(), raw.size()),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()) {
        result.error = E::overlapping_workspaces;
        return result;
    }

    const auto descriptor_size =
        static_cast<std::size_t>(layout.header.descriptor_size);
    const auto payload = serialized_frame.subspan(
        lzss_contextual_adaptive_huffman_frame_header_size + descriptor_size,
        static_cast<std::size_t>(layout.header.payload_size));
    const context::internal::LzssFieldContextValidationContext token_context{
        layout.header.token_count,
        layout.header.event_count,
        layout.header.decision_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.token_decode =
        context::internal::decode_lzss_contextual_adaptive_huffman_tokens(
            layout.descriptor, payload, context.stream.dictionary,
            token_context, context.limits, nodes, symbols, tokens,
            selected.layout.context_variant);
    if (result.token_decode.error
        != context::internal::
            LzssContextualAdaptiveHuffmanDecodeError::none) {
        result.error = E::token_decode_error;
        return result;
    }

    const dictionary::internal::LzssTypedFrameValidationContext raw_context{
        layout.header.token_count,
        layout.header.uncompressed_size,
        context.output_already_committed,
    };
    result.reconstruction = dictionary::internal::reconstruct_lzss_typed_frame(
        tokens, context.stream.dictionary, raw_context, context.limits, raw,
        selected.layout.dictionary_variant);
    if (result.reconstruction.error
        != dictionary::internal::LzssTypedReconstructError::none) {
        result.error = E::reconstruction_error;
        return result;
    }
    result.serialized_consumed = layout.serialized_size;
    return result;
}

} // namespace marc::frame::internal

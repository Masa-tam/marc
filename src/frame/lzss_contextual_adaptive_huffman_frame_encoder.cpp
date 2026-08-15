#include "frame/lzss_contextual_adaptive_huffman_frame_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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

[[nodiscard]] bool exact_input_size(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const std::uint64_t output_already_committed,
    const std::size_t input_size) noexcept {
    if (output_already_committed >= stream.original_size) return false;
    const auto remaining = stream.original_size - output_already_committed;
    const auto expected = std::min<std::uint64_t>(stream.frame_size, remaining);
    return input_size == expected && input_size != 0
        && input_size <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult fail_overlap(
    LzssContextualAdaptiveHuffmanFrameEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssContextualAdaptiveHuffmanFrameEncodeError::arithmetic_overflow
        : LzssContextualAdaptiveHuffmanFrameEncodeError::
            overlapping_workspaces;
    return result;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameHeader make_header(
    const std::uint64_t sequence, const std::size_t raw_size,
    const LzssContextualAdaptiveHuffmanFrameEncodeResult& result) noexcept {
    LzssContextualAdaptiveHuffmanFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_size);
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.event_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = static_cast<std::uint32_t>(
        result.descriptor_size);
    return header;
}

struct RegionSizes {
    std::size_t tokens{};
    std::size_t nodes{};
    std::size_t symbols{};
};

[[nodiscard]] bool calculate_region_sizes(
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const context::internal::LzssFieldContextLayout& layout,
    RegionSizes& sizes) noexcept {
    std::size_t node_entries{};
    if (!core::checked_multiply(
            layout.frequency_entries, std::size_t{2}, node_entries)
        || !core::checked_add(
            node_entries,
            static_cast<std::size_t>(
                context::internal::lzss_field_context_count),
            node_entries)) {
        return false;
    }
    return core::checked_multiply(
               tokens.size(), sizeof(dictionary::internal::LzssTypedToken),
               sizes.tokens)
        && core::checked_multiply(
            node_entries, sizeof(entropy::internal::AdaptiveHuffmanNode),
            sizes.nodes)
        && core::checked_multiply(
            layout.frequency_entries,
            sizeof(std::uint16_t), sizes.symbols);
}

[[nodiscard]] OverlapCheck find_overlap(
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols, const RegionSizes& sizes) noexcept {
    const std::array overlaps{
        regions_overlap(raw_input.data(), raw_input.size(), tokens.data(),
                        sizes.tokens),
        regions_overlap(raw_input.data(), raw_input.size(), nodes.data(),
                        sizes.nodes),
        regions_overlap(raw_input.data(), raw_input.size(), symbols.data(),
                        sizes.symbols),
        regions_overlap(tokens.data(), sizes.tokens, nodes.data(), sizes.nodes),
        regions_overlap(tokens.data(), sizes.tokens, symbols.data(),
                        sizes.symbols),
        regions_overlap(nodes.data(), sizes.nodes, symbols.data(),
                        sizes.symbols),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        return OverlapCheck::arithmetic_overflow;
    }
    return std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult plan_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    using E = LzssContextualAdaptiveHuffmanFrameEncodeError;
    LzssContextualAdaptiveHuffmanFrameEncodeResult result{};
    if (validate_lzss_contextual_adaptive_huffman_stream_header(stream, limits)
        != LzssContextualAdaptiveHuffmanStreamHeaderError::none) {
        result.error = E::invalid_stream;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = E::invalid_stream;
        return result;
    }
    result.required_node_entries =
        2 * selected.layout.frequency_entries
        + context::internal::lzss_field_context_count;
    result.required_symbol_entries = selected.layout.frequency_entries;
    if (private_nodes.size() < result.required_node_entries) {
        result.error = E::node_staging_too_small;
        return result;
    }
    if (private_symbols.size() < result.required_symbol_entries) {
        result.error = E::symbol_staging_too_small;
        return result;
    }
    RegionSizes sizes{};
    if (!calculate_region_sizes(private_tokens, selected.layout, sizes)) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    const auto overlap = find_overlap(
        raw_input, private_tokens, private_nodes, private_symbols, sizes);
    if (overlap != OverlapCheck::disjoint) {
        return fail_overlap(result, overlap);
    }
    if constexpr (UseHashChain) {
        const std::array finder_overlaps{
            regions_overlap(raw_input.data(), raw_input.size(),
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_tokens.data(), sizes.tokens,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_nodes.data(), sizes.nodes,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_symbols.data(), sizes.symbols,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
        };
        for (const auto finder_overlap : finder_overlaps) {
            if (finder_overlap != OverlapCheck::disjoint)
                return fail_overlap(result, finder_overlap);
        }
    }
    if (!exact_input_size(stream, output_already_committed,
                          raw_input.size())) {
        result.error = E::input_size_mismatch;
        return result;
    }

    if constexpr (UseHashChain) {
        result.token_encode = dictionary::internal::
            encode_lzss_typed_tokens_hash_chain_single_pass(
                raw_input, stream.dictionary, limits, private_tokens,
                match_finder_workspace, statistics,
                selected.layout.dictionary_variant);
    } else {
        result.token_encode = dictionary::internal::encode_lzss_typed_tokens(
            raw_input, stream.dictionary, limits, private_tokens,
            selected.layout.dictionary_variant);
    }
    result.token_count = result.token_encode.token_count;
    if (result.token_encode.error
        != dictionary::internal::LzssTypedEncodeError::none) {
        result.error = result.token_encode.error
                == dictionary::internal::LzssTypedEncodeError::output_too_small
            ? E::token_staging_too_small
            : E::token_encode_error;
        return result;
    }
    if (result.token_count > std::numeric_limits<std::uint32_t>::max()) {
        result.error = E::arithmetic_overflow;
        return result;
    }

    const auto tokens = private_tokens.first(result.token_count);
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()), output_already_committed};
    entropy::internal::ContextualAdaptiveHuffmanDescriptor descriptor{};
    result.entropy_encode =
        context::internal::plan_lzss_contextual_adaptive_huffman_tokens(
            tokens, stream.dictionary, token_context, limits, private_nodes,
            private_symbols, descriptor, selected.layout.context_variant);
    result.event_count = result.entropy_encode.event_count;
    result.decision_count = result.entropy_encode.decision_count;
    result.payload_size = result.entropy_encode.payload_size;
    if (result.entropy_encode.error
        != context::internal::LzssContextualAdaptiveHuffmanEncodeError::none) {
        result.error = E::entropy_encode_error;
        return result;
    }
    result.descriptor_size =
        entropy::internal::contextual_adaptive_huffman_descriptor_size;
    if (result.event_count > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    result.descriptor_error =
        entropy::internal::validate_contextual_adaptive_huffman_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits);
    if (result.descriptor_error
        != entropy::internal::ContextualAdaptiveHuffmanFormatError::none) {
        result.error = E::descriptor_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualAdaptiveHuffmanFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error =
        validate_lzss_contextual_adaptive_huffman_frame_header(
            header, frame_context);
    if (result.header_error
        != LzssContextualAdaptiveHuffmanFrameHeaderError::none) {
        result.error = E::header_error;
        return result;
    }
    if (!core::checked_add(
            lzss_contextual_adaptive_huffman_frame_header_size,
            result.descriptor_size, result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error = E::arithmetic_overflow;
        return result;
    }

    std::size_t token_workspace = result.token_encode.token_storage_size;
    if constexpr (UseHashChain) {
        if (!core::checked_multiply(
                raw_input.size(),
                sizeof(dictionary::internal::LzssTypedToken),
                token_workspace)) {
            result.error = E::arithmetic_overflow;
            return result;
        }
    }
    std::size_t workspace{};
    if (!core::checked_add(
            raw_input.size(), token_workspace, workspace)
        || !core::checked_add(workspace, sizes.nodes, workspace)
        || !core::checked_add(workspace, sizes.symbols, workspace)
        || !core::checked_add(
            workspace, result.serialized_size, workspace)) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto required = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                raw_input.size(), stream.dictionary, limits);
        if (required.error
            != dictionary::internal::LzssHashChainError::none) {
            result.error = E::token_encode_error;
            result.token_encode.error = dictionary::internal::
                LzssTypedEncodeError::match_finder_error;
            result.token_encode.match_finder_error = required.error;
            return result;
        }
        if (!core::checked_add(
                workspace, required.workspace_size, workspace)) {
            result.error = E::arithmetic_overflow;
            return result;
        }
    }
    if (workspace > limits.max_internal_buffered_bytes) {
        result.error = E::workspace_limit;
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult encode_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    using E = LzssContextualAdaptiveHuffmanFrameEncodeError;
    LzssContextualAdaptiveHuffmanFrameEncodeResult result{};
    if (validate_lzss_contextual_adaptive_huffman_stream_header(stream, limits)
        != LzssContextualAdaptiveHuffmanStreamHeaderError::none) {
        result.error = E::invalid_stream;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = E::invalid_stream;
        return result;
    }
    result.required_node_entries =
        2 * selected.layout.frequency_entries
        + context::internal::lzss_field_context_count;
    result.required_symbol_entries = selected.layout.frequency_entries;
    if (private_nodes.size() < result.required_node_entries) {
        result.error = E::node_staging_too_small;
        return result;
    }
    if (private_symbols.size() < result.required_symbol_entries) {
        result.error = E::symbol_staging_too_small;
        return result;
    }
    RegionSizes sizes{};
    if (!calculate_region_sizes(private_tokens, selected.layout, sizes)) {
        result.error = E::arithmetic_overflow;
        return result;
    }
    const std::array output_overlaps{
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        raw_input.data(), raw_input.size()),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_tokens.data(), sizes.tokens),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_nodes.data(), sizes.nodes),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_symbols.data(), sizes.symbols),
    };
    if (std::ranges::find(
            output_overlaps, OverlapCheck::arithmetic_overflow)
        != output_overlaps.end()) {
        return fail_overlap(result, OverlapCheck::arithmetic_overflow);
    }
    if (std::ranges::find(output_overlaps, OverlapCheck::overlap)
        != output_overlaps.end()) {
        return fail_overlap(result, OverlapCheck::overlap);
    }
    if constexpr (UseHashChain) {
        const auto finder_overlap = regions_overlap(
            serialized_output.data(), serialized_output.size(),
            match_finder_workspace.data(), match_finder_workspace.size());
        if (finder_overlap != OverlapCheck::disjoint)
            return fail_overlap(result, finder_overlap);
    }

    result = plan_frame<UseHashChain>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_nodes, private_symbols,
        match_finder_workspace, statistics);
    if (result.error != E::none) return result;
    if (serialized_output.size() < result.serialized_size) {
        result.error = E::serialized_output_too_small;
        return result;
    }

    const auto output = serialized_output.first(result.serialized_size);
    const auto tokens = private_tokens.first(result.token_count);
    std::size_t payload_offset{};
    if (!core::checked_add(
            lzss_contextual_adaptive_huffman_frame_header_size,
            result.descriptor_size, payload_offset)) {
        result.error = E::internal_error;
        return result;
    }
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()), output_already_committed};
    entropy::internal::ContextualAdaptiveHuffmanDescriptor descriptor{};
    result.entropy_encode =
        context::internal::encode_lzss_contextual_adaptive_huffman_tokens(
            tokens, stream.dictionary, token_context, limits, private_nodes,
            private_symbols,
            output.subspan(payload_offset, result.payload_size), descriptor,
            selected.layout.context_variant);
    if (result.entropy_encode.error
            != context::internal::
                LzssContextualAdaptiveHuffmanEncodeError::none
        || result.entropy_encode.event_count != result.event_count
        || result.entropy_encode.decision_count != result.decision_count
        || result.entropy_encode.payload_size != result.payload_size) {
        result.error = E::internal_error;
        return result;
    }

    result.descriptor_error =
        entropy::internal::serialize_contextual_adaptive_huffman_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            std::span<std::byte,
                      entropy::internal::
                          contextual_adaptive_huffman_descriptor_size>{
                output.data()
                    + lzss_contextual_adaptive_huffman_frame_header_size,
                entropy::internal::
                    contextual_adaptive_huffman_descriptor_size});
    if (result.descriptor_error
        != entropy::internal::ContextualAdaptiveHuffmanFormatError::none) {
        result.error = E::internal_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualAdaptiveHuffmanFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error =
        serialize_lzss_contextual_adaptive_huffman_frame_header(
            header, frame_context,
            std::span<std::byte,
                      lzss_contextual_adaptive_huffman_frame_header_size>{
                output.data(),
                lzss_contextual_adaptive_huffman_frame_header_size});
    if (result.header_error
        != LzssContextualAdaptiveHuffmanFrameHeaderError::none) {
        result.error = E::internal_error;
    }
    return result;
}

LzssContextualAdaptiveHuffmanFrameEncodeResult
plan_lzss_contextual_adaptive_huffman_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols) noexcept {
    return plan_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_nodes, private_symbols, {}, nullptr);
}

LzssContextualAdaptiveHuffmanFrameEncodeResult
encode_lzss_contextual_adaptive_huffman_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_nodes, private_symbols, {}, serialized_output,
        nullptr);
}

LzssContextualAdaptiveHuffmanFrameEncodeResult
plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return plan_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_nodes, private_symbols,
        match_finder_workspace, statistics);
}

LzssContextualAdaptiveHuffmanFrameEncodeResult
encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return encode_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_nodes, private_symbols,
        match_finder_workspace, serialized_output, statistics);
}

} // namespace marc::frame::internal

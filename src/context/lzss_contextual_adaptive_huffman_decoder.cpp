#include "context/lzss_contextual_adaptive_huffman_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::AdaptiveHuffmanNode;
using entropy::internal::ContextualAdaptiveHuffmanDecodeError;
using entropy::internal::ContextualAdaptiveHuffmanDecoder;
using entropy::internal::ContextualAdaptiveHuffmanDescriptor;

[[nodiscard]] bool read_symbol(
    ContextualAdaptiveHuffmanDecoder& decoder, const std::uint16_t context,
    const std::uint16_t alphabet, std::uint32_t& value,
    LzssContextualAdaptiveHuffmanDecodeResult& result) noexcept {
    result.entropy = decoder.decode_symbol(context, alphabet, value);
    if (result.entropy.error == ContextualAdaptiveHuffmanDecodeError::none) {
        return true;
    }
    result.error = LzssContextualAdaptiveHuffmanDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_bypass(
    ContextualAdaptiveHuffmanDecoder& decoder, const std::uint8_t bits,
    std::uint32_t& value,
    LzssContextualAdaptiveHuffmanDecodeResult& result) noexcept {
    result.entropy = decoder.decode_bypass(bits, value);
    if (result.entropy.error == ContextualAdaptiveHuffmanDecodeError::none) {
        return true;
    }
    result.error = LzssContextualAdaptiveHuffmanDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_token(
    ContextualAdaptiveHuffmanDecoder& decoder,
    const LzssFieldContextState& state, LzssTypedToken& token,
    LzssContextualAdaptiveHuffmanDecodeResult& result) noexcept {
    std::uint32_t kind{};
    if (!read_symbol(decoder, state.token_context(), 2, kind, result)) {
        return false;
    }
    if (kind == 0) {
        std::uint32_t literal{};
        if (!read_symbol(
                decoder, state.literal_context(), 256, literal, result)) {
            return false;
        }
        token = {LzssTypedTokenKind::literal,
                 static_cast<std::uint8_t>(literal), 0, 0};
        return true;
    }

    std::uint32_t length_class{};
    if (!read_symbol(
            decoder, state.length_context(), 8, length_class, result)) {
        return false;
    }
    std::uint32_t length_extra{};
    if (length_class != 0
        && !read_bypass(decoder, static_cast<std::uint8_t>(length_class),
                        length_extra, result)) {
        return false;
    }
    const auto length =
        (UINT32_C(1) << length_class) + length_extra + 4;

    std::uint32_t distance_class{};
    if (!read_symbol(
            decoder, LzssFieldContextState::distance_context(length_class), 17,
            distance_class, result)) {
        return false;
    }
    std::uint32_t distance_extra{};
    if (distance_class != 0
        && !read_bypass(decoder, static_cast<std::uint8_t>(distance_class),
                        distance_extra, result)) {
        return false;
    }
    const auto distance =
        (UINT32_C(1) << distance_class) + distance_extra;
    token = {LzssTypedTokenKind::match, 0, distance, length};
    return true;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck ranges_overlap(
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

[[nodiscard]] bool validate_declared_bounds(
    const std::size_t payload_size,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanDecodeResult& result) noexcept {
    const auto token_count =
        static_cast<std::uint64_t>(context.declared_token_count);
    const auto event_count =
        static_cast<std::uint64_t>(context.declared_event_count);
    const auto decision_count =
        static_cast<std::uint64_t>(context.declared_decision_count);
    const auto raw_size = static_cast<std::uint64_t>(context.declared_raw_size);
    if (token_count == 0 || raw_size == 0 || token_count > raw_size
        || event_count < 2 * token_count || event_count > 5 * token_count
        || event_count > 2 * raw_size || decision_count < event_count
        || decision_count > 26 * token_count
        || decision_count > 6 * raw_size) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::invalid_counts;
        return false;
    }
    std::size_t token_bytes{};
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    std::size_t model_bytes{};
    std::size_t aggregate_bytes{};
    if (!core::checked_multiply(
            static_cast<std::size_t>(context.declared_token_count),
            sizeof(LzssTypedToken), token_bytes)
        || !core::checked_multiply(
            entropy::internal::contextual_adaptive_huffman_node_entries,
            sizeof(AdaptiveHuffmanNode), node_bytes)
        || !core::checked_multiply(
            entropy::internal::contextual_adaptive_huffman_symbol_entries,
            sizeof(std::uint16_t), symbol_bytes)
        || !core::checked_add(token_bytes, node_bytes, model_bytes)
        || !core::checked_add(model_bytes, symbol_bytes, model_bytes)
        || !core::checked_add(model_bytes, payload_size, aggregate_bytes)) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
        return false;
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes
        || raw_size > limits.max_frame_size
        || raw_size > limits.max_block_size) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded;
        return false;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(
            context.output_already_committed, raw_size, total_output)) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
        return false;
    }
    if (total_output > limits.max_total_output_size) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded;
        return false;
    }
    return true;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanDecodeResult preflight_workspace(
    const std::span<const std::byte> payload,
    const std::span<AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    std::span<AdaptiveHuffmanNode>& nodes,
    std::span<std::uint16_t>& symbols) noexcept {
    LzssContextualAdaptiveHuffmanDecodeResult result{};
    if (private_nodes.size()
        < entropy::internal::contextual_adaptive_huffman_node_entries) {
        result.error = LzssContextualAdaptiveHuffmanDecodeError::
            node_workspace_too_small;
        return result;
    }
    if (private_symbols.size()
        < entropy::internal::contextual_adaptive_huffman_symbol_entries) {
        result.error = LzssContextualAdaptiveHuffmanDecodeError::
            symbol_workspace_too_small;
        return result;
    }
    nodes = private_nodes.first(
        entropy::internal::contextual_adaptive_huffman_node_entries);
    symbols = private_symbols.first(
        entropy::internal::contextual_adaptive_huffman_symbol_entries);
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    if (!core::checked_multiply(nodes.size(), sizeof(AdaptiveHuffmanNode),
                                node_bytes)
        || !core::checked_multiply(symbols.size(), sizeof(std::uint16_t),
                                   symbol_bytes)) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
        return result;
    }
    const auto payload_nodes = ranges_overlap(
        payload.data(), payload.size(), nodes.data(), node_bytes);
    const auto payload_symbols = ranges_overlap(
        payload.data(), payload.size(), symbols.data(), symbol_bytes);
    const auto nodes_symbols = ranges_overlap(
        nodes.data(), node_bytes, symbols.data(), symbol_bytes);
    if (payload_nodes == OverlapCheck::arithmetic_overflow
        || payload_symbols == OverlapCheck::arithmetic_overflow
        || nodes_symbols == OverlapCheck::arithmetic_overflow) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
    } else if (payload_nodes == OverlapCheck::overlap
               || payload_symbols == OverlapCheck::overlap
               || nodes_symbols == OverlapCheck::overlap) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanDecodeResult preflight_tokens(
    const std::span<const std::byte> payload,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<LzssTypedToken> private_tokens,
    const std::uint32_t token_count,
    std::span<LzssTypedToken>& tokens) noexcept {
    LzssContextualAdaptiveHuffmanDecodeResult result{};
    if (private_tokens.size() < token_count) return result;
    tokens = private_tokens.first(token_count);
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    std::size_t token_bytes{};
    if (!core::checked_multiply(nodes.size(), sizeof(AdaptiveHuffmanNode),
                                node_bytes)
        || !core::checked_multiply(symbols.size(), sizeof(std::uint16_t),
                                   symbol_bytes)
        || !core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                   token_bytes)) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
        return result;
    }
    const auto payload_tokens = ranges_overlap(
        payload.data(), payload.size(), tokens.data(), token_bytes);
    const auto nodes_tokens = ranges_overlap(
        nodes.data(), node_bytes, tokens.data(), token_bytes);
    const auto symbols_tokens = ranges_overlap(
        symbols.data(), symbol_bytes, tokens.data(), token_bytes);
    if (payload_tokens == OverlapCheck::arithmetic_overflow
        || nodes_tokens == OverlapCheck::arithmetic_overflow
        || symbols_tokens == OverlapCheck::arithmetic_overflow) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::arithmetic_overflow;
    } else if (payload_tokens == OverlapCheck::overlap
               || nodes_tokens == OverlapCheck::overlap
               || symbols_tokens == OverlapCheck::overlap) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanDecodeResult run_pass(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<LzssTypedToken> output) noexcept {
    LzssContextualAdaptiveHuffmanDecodeResult result{};
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded
            : LzssContextualAdaptiveHuffmanDecodeError::invalid_parameters;
        return result;
    }
    if (!validate_declared_bounds(payload.size(), context, limits, result)) {
        return result;
    }
    if (descriptor.decision_count != context.declared_decision_count) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::invalid_counts;
        return result;
    }

    ContextualAdaptiveHuffmanDecoder decoder;
    result.entropy = decoder.begin(
        descriptor, payload, limits, nodes, symbols);
    if (result.entropy.error != ContextualAdaptiveHuffmanDecodeError::none) {
        result.error = LzssContextualAdaptiveHuffmanDecodeError::entropy_error;
        return result;
    }

    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        LzssTypedToken token{};
        if (!read_token(decoder, state, token, result)) return result;
        std::uint64_t next_raw_size{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters, {result.raw_size, context.declared_raw_size},
            limits, next_raw_size);
        if (result.token_error != LzssTypedTokenError::none) {
            if (result.token_error == LzssTypedTokenError::limit_exceeded) {
                result.error =
                    LzssContextualAdaptiveHuffmanDecodeError::limit_exceeded;
            } else if (result.token_error
                       == LzssTypedTokenError::arithmetic_overflow) {
                result.error = LzssContextualAdaptiveHuffmanDecodeError::
                    arithmetic_overflow;
            } else {
                result.error =
                    LzssContextualAdaptiveHuffmanDecodeError::invalid_token;
            }
            return result;
        }
        if (!output.empty()) output[result.token_count] = token;
        result.raw_size = next_raw_size;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.entropy = decoder.finish(
        context.declared_event_count, context.declared_decision_count);
    if (result.entropy.error != ContextualAdaptiveHuffmanDecodeError::none) {
        result.error = LzssContextualAdaptiveHuffmanDecodeError::entropy_error;
        return result;
    }
    if (result.raw_size != context.declared_raw_size) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::raw_size_mismatch;
    }
    return result;
}

} // namespace

LzssContextualAdaptiveHuffmanDecodeResult
validate_lzss_contextual_adaptive_huffman_tokens(
    const entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols) noexcept {
    std::span<AdaptiveHuffmanNode> nodes{};
    std::span<std::uint16_t> symbols{};
    const auto workspace = preflight_workspace(
        payload, private_nodes, private_symbols, nodes, symbols);
    if (workspace.error
        != LzssContextualAdaptiveHuffmanDecodeError::none) {
        return workspace;
    }
    return run_pass(
        descriptor, payload, parameters, context, limits, nodes, symbols, {});
}

LzssContextualAdaptiveHuffmanDecodeResult
decode_lzss_contextual_adaptive_huffman_tokens(
    const entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    const std::span<std::uint16_t> private_symbols,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    std::span<AdaptiveHuffmanNode> nodes{};
    std::span<std::uint16_t> symbols{};
    const auto workspace = preflight_workspace(
        payload, private_nodes, private_symbols, nodes, symbols);
    if (workspace.error
        != LzssContextualAdaptiveHuffmanDecodeError::none) {
        return workspace;
    }
    std::span<LzssTypedToken> tokens{};
    const auto token_workspace = preflight_tokens(
        payload, nodes, symbols, private_tokens, context.declared_token_count,
        tokens);
    if (token_workspace.error
        != LzssContextualAdaptiveHuffmanDecodeError::none) {
        return token_workspace;
    }

    auto result = run_pass(
        descriptor, payload, parameters, context, limits, nodes, symbols, {});
    if (result.error
        != LzssContextualAdaptiveHuffmanDecodeError::none) {
        return result;
    }
    if (private_tokens.size() < context.declared_token_count) {
        result.error = LzssContextualAdaptiveHuffmanDecodeError::
            token_output_too_small;
        return result;
    }
    const auto decoded = run_pass(
        descriptor, payload, parameters, context, limits, nodes, symbols,
        tokens);
    if (decoded.error != LzssContextualAdaptiveHuffmanDecodeError::none
        || decoded.token_count != result.token_count
        || decoded.raw_size != result.raw_size
        || decoded.entropy.event_count != result.entropy.event_count
        || decoded.entropy.decision_count != result.entropy.decision_count
        || decoded.entropy.bits_consumed != result.entropy.bits_consumed) {
        result.error =
            LzssContextualAdaptiveHuffmanDecodeError::internal_error;
        return result;
    }
    return decoded;
}

} // namespace marc::context::internal

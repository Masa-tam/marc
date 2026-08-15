#include "context/lzss_contextual_adaptive_huffman_encoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedFrameValidationError;
using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::AdaptiveHuffmanNode;
using entropy::internal::ContextualAdaptiveHuffmanEncodeError;
using entropy::internal::ContextualAdaptiveHuffmanForwardEncoder;

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

[[nodiscard]] LzssContextualAdaptiveHuffmanEncodeResult fail_entropy(
    LzssContextualAdaptiveHuffmanEncodeResult result,
    const entropy::internal::ContextualAdaptiveHuffmanEncodeResult& entropy)
    noexcept {
    result.entropy = entropy;
    if (entropy.error
        == ContextualAdaptiveHuffmanEncodeError::node_workspace_too_small) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            node_workspace_too_small;
    } else if (entropy.error
               == ContextualAdaptiveHuffmanEncodeError::
                   symbol_workspace_too_small) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            symbol_workspace_too_small;
    } else if (entropy.error
               == ContextualAdaptiveHuffmanEncodeError::
                   payload_output_too_small) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            payload_output_too_small;
    } else if (entropy.error
               == ContextualAdaptiveHuffmanEncodeError::overlapping_buffers) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::overlapping_buffers;
    } else if (entropy.error
               == ContextualAdaptiveHuffmanEncodeError::limit_exceeded) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded;
    } else if (entropy.error
               == ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
    } else {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::entropy_error;
    }
    return result;
}

[[nodiscard]] std::uint8_t value_class(
    const std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

[[nodiscard]] bool emit_token(
    ContextualAdaptiveHuffmanForwardEncoder& encoder,
    const LzssFieldContextState& state, const LzssTypedToken& token,
    LzssContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    const auto kind = token.kind == LzssTypedTokenKind::literal ? 0U : 1U;
    result.entropy = encoder.encode_symbol(state.token_context(), 2, kind);
    if (result.entropy.error != ContextualAdaptiveHuffmanEncodeError::none) {
        result = fail_entropy(result, result.entropy);
        return false;
    }
    if (token.kind == LzssTypedTokenKind::literal) {
        result.entropy = encoder.encode_symbol(
            state.literal_context(), 256, token.literal);
        if (result.entropy.error
            != ContextualAdaptiveHuffmanEncodeError::none) {
            result = fail_entropy(result, result.entropy);
            return false;
        }
        return true;
    }
    const auto length_value = token.length - 4U;
    const auto length_class = value_class(length_value);
    const auto length_extra =
        length_value - (UINT32_C(1) << length_class);
    result.entropy = encoder.encode_symbol(
        state.length_context(), 8, length_class);
    if (result.entropy.error != ContextualAdaptiveHuffmanEncodeError::none) {
        result = fail_entropy(result, result.entropy);
        return false;
    }
    if (length_class != 0) {
        result.entropy = encoder.encode_bypass(length_class, length_extra);
        if (result.entropy.error
            != ContextualAdaptiveHuffmanEncodeError::none) {
            result = fail_entropy(result, result.entropy);
            return false;
        }
    }
    const auto distance_class = value_class(token.distance);
    const auto distance_extra =
        token.distance - (UINT32_C(1) << distance_class);
    result.entropy = encoder.encode_symbol(
        LzssFieldContextState::distance_context(length_class), 17,
        distance_class);
    if (result.entropy.error != ContextualAdaptiveHuffmanEncodeError::none) {
        result = fail_entropy(result, result.entropy);
        return false;
    }
    if (distance_class != 0) {
        result.entropy = encoder.encode_bypass(
            distance_class, distance_extra);
        if (result.entropy.error
            != ContextualAdaptiveHuffmanEncodeError::none) {
            result = fail_entropy(result, result.entropy);
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool run_tokens(
    ContextualAdaptiveHuffmanForwardEncoder& encoder,
    const std::span<const LzssTypedToken> tokens,
    LzssContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    LzssFieldContextState state{};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        result.token_index = index;
        if (!emit_token(encoder, state, tokens[index], result)) return false;
        state.accept(tokens[index]);
        ++result.token_count;
    }
    result.token_index = result.token_count;
    return true;
}

struct Extents {
    std::size_t token_bytes{};
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
};

[[nodiscard]] bool calculate_extents(
    const std::span<const LzssTypedToken> tokens,
    Extents& extents) noexcept {
    return core::checked_multiply(
               tokens.size(), sizeof(LzssTypedToken), extents.token_bytes)
        && core::checked_multiply(
            entropy::internal::contextual_adaptive_huffman_node_entries,
            sizeof(AdaptiveHuffmanNode), extents.node_bytes)
        && core::checked_multiply(
            entropy::internal::contextual_adaptive_huffman_symbol_entries,
            sizeof(std::uint16_t), extents.symbol_bytes);
}

[[nodiscard]] LzssContextualAdaptiveHuffmanEncodeError validate_token_regions(
    const std::span<const LzssTypedToken> tokens,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<std::byte> payload,
    const Extents& extents) noexcept {
    const std::array overlaps{
        ranges_overlap(
            tokens.data(), extents.token_bytes,
            nodes.data(), extents.node_bytes),
        ranges_overlap(
            tokens.data(), extents.token_bytes,
            symbols.data(), extents.symbol_bytes),
        ranges_overlap(
            tokens.data(), extents.token_bytes,
            payload.data(), payload.size()),
    };
    if (std::ranges::find(
            overlaps, OverlapCheck::arithmetic_overflow) != overlaps.end()) {
        return LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()) {
        return LzssContextualAdaptiveHuffmanEncodeError::overlapping_buffers;
    }
    return LzssContextualAdaptiveHuffmanEncodeError::none;
}

[[nodiscard]] bool validate_aggregate(
    const Extents& extents, const std::size_t payload_size,
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    std::size_t aggregate{};
    if (!core::checked_add(extents.token_bytes, extents.node_bytes, aggregate)
        || !core::checked_add(aggregate, extents.symbol_bytes, aggregate)
        || !core::checked_add(aggregate, payload_size, aggregate)) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
        return false;
    }
    if (aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded;
        return false;
    }
    return true;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanEncodeResult validate_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    LzssContextualAdaptiveHuffmanEncodeResult result{};
    result.token_validation = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits);
    result.token_count = result.token_validation.token_count;
    result.token_index = result.token_validation.token_index;
    if (result.token_validation.error == LzssTypedFrameValidationError::none) {
        return result;
    }
    if (result.token_validation.error
        == LzssTypedFrameValidationError::limit_exceeded) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::limit_exceeded;
    } else if (result.token_validation.error
               == LzssTypedFrameValidationError::arithmetic_overflow) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
    } else if (result.token_validation.error
               == LzssTypedFrameValidationError::invalid_parameters) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::invalid_parameters;
    } else {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::invalid_token;
    }
    return result;
}

} // namespace

LzssContextualAdaptiveHuffmanEncodeResult
plan_lzss_contextual_adaptive_huffman_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor)
    noexcept {
    auto result = validate_tokens(tokens, parameters, context, limits);
    if (result.error != LzssContextualAdaptiveHuffmanEncodeError::none) {
        return result;
    }
    Extents extents{};
    if (!calculate_extents(tokens, extents)) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    if (node_workspace.size()
        < entropy::internal::contextual_adaptive_huffman_node_entries) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            node_workspace_too_small;
        return result;
    }
    if (symbol_workspace.size()
        < entropy::internal::contextual_adaptive_huffman_symbol_entries) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            symbol_workspace_too_small;
        return result;
    }
    const auto region_error = validate_token_regions(
        tokens, node_workspace, symbol_workspace, {}, extents);
    if (region_error != LzssContextualAdaptiveHuffmanEncodeError::none) {
        result.error = region_error;
        return result;
    }
    result.token_count = 0;
    result.token_index = 0;
    ContextualAdaptiveHuffmanForwardEncoder encoder;
    result.entropy = encoder.begin_plan(
        limits, node_workspace, symbol_workspace,
        LzssFieldContextVariant::field_context_64k);
    if (result.entropy.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail_entropy(result, result.entropy);
    }
    if (!run_tokens(encoder, tokens, result)) return result;
    entropy::internal::ContextualAdaptiveHuffmanDescriptor planned{};
    result.entropy = encoder.finish_plan(planned);
    if (result.entropy.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail_entropy(result, result.entropy);
    }
    result.event_count = result.entropy.operation_count;
    result.decision_count = result.entropy.decision_count;
    result.payload_size = result.entropy.payload_size;
    result.payload_bits = result.entropy.payload_bits;
    if (!validate_aggregate(extents, result.payload_size, limits, result)) {
        return result;
    }
    descriptor = planned;
    return result;
}

LzssContextualAdaptiveHuffmanEncodeResult
encode_lzss_contextual_adaptive_huffman_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor)
    noexcept {
    entropy::internal::ContextualAdaptiveHuffmanDescriptor planned{};
    auto result = plan_lzss_contextual_adaptive_huffman_tokens(
        tokens, parameters, context, limits, node_workspace, symbol_workspace,
        planned);
    if (result.error != LzssContextualAdaptiveHuffmanEncodeError::none) {
        return result;
    }
    if (payload_output.size() < result.payload_size) {
        result.error = LzssContextualAdaptiveHuffmanEncodeError::
            payload_output_too_small;
        return result;
    }
    const auto payload = payload_output.first(result.payload_size);
    Extents extents{};
    if (!calculate_extents(tokens, extents)) {
        result.error =
            LzssContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    const auto region_error = validate_token_regions(
        tokens, node_workspace, symbol_workspace, payload, extents);
    if (region_error != LzssContextualAdaptiveHuffmanEncodeError::none) {
        result.error = region_error;
        return result;
    }
    ContextualAdaptiveHuffmanForwardEncoder encoder;
    auto entropy_result = encoder.begin_write(
        planned, limits, node_workspace, symbol_workspace, payload,
        LzssFieldContextVariant::field_context_64k);
    if (entropy_result.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail_entropy(result, entropy_result);
    }
    const auto expected_tokens = result.token_count;
    const auto expected_events = result.event_count;
    const auto expected_decisions = result.decision_count;
    const auto expected_bits = result.payload_bits;
    result.token_count = 0;
    result.token_index = 0;
    if (!run_tokens(encoder, tokens, result)) return result;
    entropy_result = encoder.finish_write(
        expected_events, expected_decisions, expected_bits);
    if (entropy_result.error != ContextualAdaptiveHuffmanEncodeError::none
        || result.token_count != expected_tokens) {
        return fail_entropy(result, entropy_result.error
                == ContextualAdaptiveHuffmanEncodeError::none
            ? entropy::internal::ContextualAdaptiveHuffmanEncodeResult{
                  entropy_result.operation_count,
                  entropy_result.operation_index,
                  entropy_result.decision_count,
                  entropy_result.payload_size,
                  entropy_result.payload_bits,
                  ContextualAdaptiveHuffmanEncodeError::internal_error}
            : entropy_result);
    }
    result.entropy = entropy_result;
    result.event_count = entropy_result.operation_count;
    result.decision_count = entropy_result.decision_count;
    result.payload_size = entropy_result.payload_size;
    result.payload_bits = entropy_result.payload_bits;
    descriptor = planned;
    return result;
}

} // namespace marc::context::internal

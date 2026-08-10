#include "context/lzss_contextual_blocked_huffman_encoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualBlockedHuffmanEncodeError;
using entropy::internal::ContextualBlockedHuffmanModelBuilder;
using entropy::internal::ContextualBlockedHuffmanWriter;

[[nodiscard]] std::uint8_t value_class(
    const std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

[[nodiscard]] LzssContextualBlockedHuffmanEncodeResult fail_entropy(
    LzssContextualBlockedHuffmanEncodeResult result,
    const ContextualBlockedHuffmanEncodeError error) noexcept {
    result.entropy_error = error;
    if (error == ContextualBlockedHuffmanEncodeError::limit_exceeded) {
        result.error = LzssContextualBlockedHuffmanEncodeError::limit_exceeded;
    } else if (error
               == ContextualBlockedHuffmanEncodeError::arithmetic_overflow) {
        result.error =
            LzssContextualBlockedHuffmanEncodeError::arithmetic_overflow;
    } else if (error
               == ContextualBlockedHuffmanEncodeError::
                   payload_output_too_small) {
        result.error =
            LzssContextualBlockedHuffmanEncodeError::payload_output_too_small;
    } else {
        result.error = LzssContextualBlockedHuffmanEncodeError::entropy_error;
    }
    return result;
}

[[nodiscard]] bool add_event(
    LzssContextualBlockedHuffmanEncodeResult& result) noexcept {
    if (result.event_count == std::numeric_limits<std::size_t>::max()) {
        result.error =
            LzssContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return false;
    }
    ++result.event_count;
    return true;
}

template <typename Sink>
[[nodiscard]] ContextualBlockedHuffmanEncodeError emit_symbol(
    Sink& sink, const std::uint16_t context_id,
    const std::uint16_t alphabet, const std::uint32_t value) noexcept {
    return sink.encode_symbol(context_id, alphabet, value);
}

template <>
ContextualBlockedHuffmanEncodeError emit_symbol(
    ContextualBlockedHuffmanModelBuilder& sink,
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value) noexcept {
    return sink.add_symbol(context_id, alphabet, value);
}

template <typename Sink>
[[nodiscard]] ContextualBlockedHuffmanEncodeError emit_bypass(
    Sink& sink, const std::uint8_t bit_count,
    const std::uint32_t value) noexcept {
    return bit_count == 0
        ? ContextualBlockedHuffmanEncodeError::none
        : sink.encode_bypass(bit_count, value);
}

template <>
ContextualBlockedHuffmanEncodeError emit_bypass(
    ContextualBlockedHuffmanModelBuilder& sink,
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    return bit_count == 0
        ? ContextualBlockedHuffmanEncodeError::none
        : sink.add_bypass(bit_count, value);
}

template <typename Sink>
[[nodiscard]] ContextualBlockedHuffmanEncodeError emit_token(
    Sink& sink, const LzssFieldContextState& state,
    const LzssTypedToken& token, std::size_t& events) noexcept {
    auto emit = [&](const auto error) {
        if (error != ContextualBlockedHuffmanEncodeError::none) return error;
        if (events == std::numeric_limits<std::size_t>::max()) {
            return ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        }
        ++events;
        return ContextualBlockedHuffmanEncodeError::none;
    };
    const auto kind = token.kind == LzssTypedTokenKind::literal ? 0U : 1U;
    auto error = emit(emit_symbol(sink, state.token_context(), 2, kind));
    if (error != ContextualBlockedHuffmanEncodeError::none) return error;
    if (token.kind == LzssTypedTokenKind::literal) {
        return emit(emit_symbol(
            sink, state.literal_context(), 256, token.literal));
    }
    const auto length_value = token.length - 4U;
    const auto length_class = value_class(length_value);
    const auto length_extra =
        length_value - (UINT32_C(1) << length_class);
    const auto distance_class = value_class(token.distance);
    const auto distance_extra =
        token.distance - (UINT32_C(1) << distance_class);
    error = emit(emit_symbol(
        sink, state.length_context(), 8, length_class));
    if (error != ContextualBlockedHuffmanEncodeError::none) return error;
    if (length_class != 0) {
        error = emit(emit_bypass(
            sink, length_class, length_extra));
        if (error != ContextualBlockedHuffmanEncodeError::none) return error;
    }
    error = emit(emit_symbol(
        sink, LzssFieldContextState::distance_context(length_class), 17,
        distance_class));
    if (error != ContextualBlockedHuffmanEncodeError::none) return error;
    return distance_class == 0
        ? ContextualBlockedHuffmanEncodeError::none
        : emit(emit_bypass(sink, distance_class, distance_extra));
}

[[nodiscard]] bool ranges_overlap(
    const void* first, const std::size_t first_size, const void* second,
    const std::size_t second_size, bool& overflow) noexcept {
    if (first_size == 0 || second_size == 0) return false;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        overflow = true;
        return false;
    }
    return first_begin < second_end && second_begin < first_end;
}

} // namespace

LzssContextualBlockedHuffmanEncodeResult
plan_lzss_contextual_blocked_huffman_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor) noexcept {
    LzssContextualBlockedHuffmanEncodeResult result{};
    result.token_validation = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits);
    result.token_count = result.token_validation.token_count;
    result.token_index = result.token_validation.token_index;
    if (result.token_validation.error
        != dictionary::internal::LzssTypedFrameValidationError::none) {
        result.error = result.token_validation.error
                == dictionary::internal::LzssTypedFrameValidationError::
                    arithmetic_overflow
            ? LzssContextualBlockedHuffmanEncodeError::arithmetic_overflow
            : result.token_validation.error
                    == dictionary::internal::LzssTypedFrameValidationError::
                        limit_exceeded
                ? LzssContextualBlockedHuffmanEncodeError::limit_exceeded
                : LzssContextualBlockedHuffmanEncodeError::
                    token_validation_error;
        return result;
    }

    ContextualBlockedHuffmanModelBuilder builder;
    LzssFieldContextState state{};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        result.token_index = index;
        const auto error = emit_token(
            builder, state, tokens[index], result.event_count);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            return fail_entropy(result, error);
        }
        state.accept(tokens[index]);
    }
    result.token_index = tokens.size();
    entropy::internal::ContextualBlockedHuffmanDescriptor planned{};
    const auto entropy_result = builder.finish(limits, planned);
    if (entropy_result.error != ContextualBlockedHuffmanEncodeError::none) {
        return fail_entropy(result, entropy_result.error);
    }
    result.decision_count = entropy_result.decision_count;
    result.descriptor_size = entropy_result.descriptor_size;
    result.payload_size = entropy_result.payload_size;
    descriptor = planned;
    return result;
}

LzssContextualBlockedHuffmanEncodeResult
encode_lzss_contextual_blocked_huffman_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor) noexcept {
    LzssContextualBlockedHuffmanEncodeResult initial{};
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            tokens.size(), sizeof(LzssTypedToken), token_bytes)) {
        initial.error =
            LzssContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return initial;
    }
    bool overlap_overflow{};
    if (ranges_overlap(
            tokens.data(), token_bytes, payload_output.data(),
            payload_output.size(), overlap_overflow)
        || overlap_overflow) {
        initial.error = overlap_overflow
            ? LzssContextualBlockedHuffmanEncodeError::arithmetic_overflow
            : LzssContextualBlockedHuffmanEncodeError::overlapping_buffers;
        return initial;
    }
    entropy::internal::ContextualBlockedHuffmanDescriptor planned{};
    auto result = plan_lzss_contextual_blocked_huffman_tokens(
        tokens, parameters, context, limits, planned);
    if (result.error != LzssContextualBlockedHuffmanEncodeError::none) {
        return result;
    }
    if (payload_output.size() < result.payload_size) {
        result.error =
            LzssContextualBlockedHuffmanEncodeError::payload_output_too_small;
        return result;
    }
    ContextualBlockedHuffmanWriter writer(planned, payload_output);
    LzssFieldContextState state{};
    std::size_t events{};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        result.token_index = index;
        const auto error = emit_token(writer, state, tokens[index], events);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            return fail_entropy(result, error);
        }
        state.accept(tokens[index]);
    }
    const auto finish_error = writer.finish(events, result.decision_count);
    if (finish_error != ContextualBlockedHuffmanEncodeError::none
        || events != result.event_count) {
        return fail_entropy(
            result, finish_error == ContextualBlockedHuffmanEncodeError::none
                ? ContextualBlockedHuffmanEncodeError::internal_error
                : finish_error);
    }
    result.token_index = tokens.size();
    descriptor = planned;
    return result;
}

} // namespace marc::context::internal

#include "context/lzss_contextual_range_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualDynamicRangeDecodeError;
using entropy::internal::ContextualDynamicRangeDecoder;
using entropy::internal::ContextualDynamicRangeDescriptor;

[[nodiscard]] bool read_symbol(
    ContextualDynamicRangeDecoder& decoder, const std::uint16_t context,
    const std::uint16_t alphabet, std::uint32_t& value,
    LzssContextualRangeDecodeResult& result) noexcept {
    result.entropy = decoder.decode_symbol(context, alphabet, value);
    if (result.entropy.error == ContextualDynamicRangeDecodeError::none) {
        return true;
    }
    result.error = LzssContextualRangeDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_bypass(
    ContextualDynamicRangeDecoder& decoder, const std::uint8_t bits,
    std::uint32_t& value,
    LzssContextualRangeDecodeResult& result) noexcept {
    result.entropy = decoder.decode_bypass(bits, value);
    if (result.entropy.error == ContextualDynamicRangeDecodeError::none) {
        return true;
    }
    result.error = LzssContextualRangeDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_token(
    ContextualDynamicRangeDecoder& decoder,
    const LzssFieldContextState& state,
    const LzssFieldContextLayout& layout,
    LzssTypedToken& token,
    LzssContextualRangeDecodeResult& result) noexcept {
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
            decoder, LzssFieldContextState::distance_context(length_class),
            (*layout.alphabets)[LzssFieldContextState::distance_context(
                length_class)],
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

[[nodiscard]] bool validate_declared_bounds(
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const LzssFieldContextLayout& layout,
    LzssContextualRangeDecodeResult& result) noexcept {
    const auto token_count =
        static_cast<std::uint64_t>(context.declared_token_count);
    const auto event_count =
        static_cast<std::uint64_t>(context.declared_event_count);
    const auto decision_count =
        static_cast<std::uint64_t>(context.declared_decision_count);
    const auto raw_size =
        static_cast<std::uint64_t>(context.declared_raw_size);
    if (token_count == 0 || raw_size == 0 || token_count > raw_size
        || event_count < 2 * token_count || event_count > 5 * token_count
        || event_count > 2 * raw_size || decision_count < event_count
        || decision_count
               > layout.maximum_decisions_per_token * token_count
        || decision_count
               > layout.maximum_decisions_per_raw_byte * raw_size) {
        result.error = LzssContextualRangeDecodeError::invalid_counts;
        return false;
    }
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            static_cast<std::size_t>(context.declared_token_count),
            sizeof(LzssTypedToken), token_bytes)) {
        result.error = LzssContextualRangeDecodeError::arithmetic_overflow;
        return false;
    }
    if (token_bytes > limits.max_internal_buffered_bytes
        || raw_size > limits.max_frame_size
        || raw_size > limits.max_block_size) {
        result.error = LzssContextualRangeDecodeError::limit_exceeded;
        return false;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(
            context.output_already_committed, raw_size, total_output)) {
        result.error = LzssContextualRangeDecodeError::arithmetic_overflow;
        return false;
    }
    if (total_output > limits.max_total_output_size) {
        result.error = LzssContextualRangeDecodeError::limit_exceeded;
        return false;
    }
    return true;
}

[[nodiscard]] LzssContextualRangeDecodeResult run_pass(
    const ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> output,
    const LzssFieldContextVariant variant) noexcept {
    LzssContextualRangeDecodeResult result{};
    const auto selected = get_lzss_field_context_layout(variant);
    if (selected.error != LzssFieldContextLayoutError::none) {
        result.error = LzssContextualRangeDecodeError::invalid_parameters;
        return result;
    }
    const auto& layout = selected.layout;
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits, layout.dictionary_variant);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssContextualRangeDecodeError::limit_exceeded
            : LzssContextualRangeDecodeError::invalid_parameters;
        return result;
    }
    if (!validate_declared_bounds(context, limits, layout, result)) {
        return result;
    }
    if (descriptor.decision_count != context.declared_decision_count) {
        result.error = LzssContextualRangeDecodeError::invalid_counts;
        return result;
    }

    ContextualDynamicRangeDecoder decoder;
    result.entropy = decoder.begin(descriptor, payload, limits, variant);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
        return result;
    }

    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        LzssTypedToken token{};
        if (!read_token(decoder, state, layout, token, result)) return result;

        std::uint64_t next_raw_size{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters,
            {result.raw_size, context.declared_raw_size}, limits,
            next_raw_size, layout.dictionary_variant);
        if (result.token_error != LzssTypedTokenError::none) {
            if (result.token_error == LzssTypedTokenError::limit_exceeded) {
                result.error = LzssContextualRangeDecodeError::limit_exceeded;
            } else if (result.token_error
                       == LzssTypedTokenError::arithmetic_overflow) {
                result.error =
                    LzssContextualRangeDecodeError::arithmetic_overflow;
            } else {
                result.error = LzssContextualRangeDecodeError::invalid_token;
            }
            return result;
        }
        if (!output.empty()) output[result.token_count] = token;
        result.raw_size = next_raw_size;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.entropy = decoder.finish(context.declared_event_count,
                                    context.declared_decision_count);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
        return result;
    }
    if (result.raw_size != context.declared_raw_size) {
        result.error = LzssContextualRangeDecodeError::raw_size_mismatch;
    }
    return result;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck payload_token_overlap(
    const std::span<const std::byte> payload,
    const std::span<LzssTypedToken> tokens) noexcept {
    if (payload.empty() || tokens.empty()) return OverlapCheck::disjoint;
    std::size_t token_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto payload_begin =
        reinterpret_cast<std::uintptr_t>(payload.data());
    const auto token_begin = reinterpret_cast<std::uintptr_t>(tokens.data());
    std::uintptr_t payload_end{};
    std::uintptr_t token_end{};
    if (!core::checked_add(payload_begin,
                           static_cast<std::uintptr_t>(payload.size()),
                           payload_end)
        || !core::checked_add(token_begin,
                              static_cast<std::uintptr_t>(token_bytes),
                              token_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return payload_begin < token_end && token_begin < payload_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssContextualRangeDecodeResult validate_lzss_contextual_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const LzssFieldContextVariant variant) noexcept {
    return run_pass(
        descriptor, payload, parameters, context, limits, {}, variant);
}

LzssContextualRangeDecodeResult decode_lzss_contextual_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const LzssFieldContextVariant variant) noexcept {
    auto result = run_pass(
        descriptor, payload, parameters, context, limits, {}, variant);
    if (result.error != LzssContextualRangeDecodeError::none) return result;
    if (private_tokens.size() < context.declared_token_count) {
        result.error = LzssContextualRangeDecodeError::output_too_small;
        return result;
    }
    const auto output = private_tokens.first(context.declared_token_count);
    const auto overlap = payload_token_overlap(payload, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error = LzssContextualRangeDecodeError::arithmetic_overflow;
        return result;
    }
    if (overlap == OverlapCheck::overlap) {
        result.error = LzssContextualRangeDecodeError::overlapping_buffers;
        return result;
    }
    const auto decoded = run_pass(
        descriptor, payload, parameters, context, limits, output, variant);
    if (decoded.error != LzssContextualRangeDecodeError::none
        || decoded.token_count != result.token_count
        || decoded.raw_size != result.raw_size
        || decoded.entropy.event_count != result.entropy.event_count
        || decoded.entropy.decision_count != result.entropy.decision_count
        || decoded.entropy.payload_consumed
               != result.entropy.payload_consumed) {
        result.error = LzssContextualRangeDecodeError::internal_error;
        return result;
    }
    return decoded;
}

} // namespace marc::context::internal

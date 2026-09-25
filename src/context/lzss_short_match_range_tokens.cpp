#include "context/lzss_short_match_range_tokens.hpp"
#include "context/lzss_short_length_escape_range_tokens.hpp"
#include "context/lzss_reduced_literal_range_tokens.hpp"
#include "entropy/lzss_reduced_literal_range_decoder.hpp"
#include "context/lzss_position_distance_range_tokens.hpp"
#include "entropy/lzss_position_distance_range_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/checked_math.hpp"
#include "entropy/lzss_short_match_range_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualDynamicRangeDecodeError;
using entropy::internal::LzssShortMatchRangeDecoder;

enum class LengthMapping : std::uint8_t {
    shifted_two,
    isolated_escape,
    reduced_literal,
};

[[nodiscard]] constexpr dictionary::internal::LzssTypedTokenVariant
token_variant(const LengthMapping mapping) noexcept {
    return mapping != LengthMapping::shifted_two
        ? dictionary::internal::LzssTypedTokenVariant::
              field_context_64k_short_length_escape
        : dictionary::internal::LzssTypedTokenVariant::
              field_context_64k_short_match;
}

template<class Decoder>
[[nodiscard]] bool read_symbol(
    Decoder& decoder, const std::uint16_t context_id,
    const std::uint16_t alphabet, std::uint32_t& value,
    LzssContextualRangeDecodeResult& result) noexcept {
    result.entropy = decoder.decode_symbol(context_id, alphabet, value);
    if (result.entropy.error == ContextualDynamicRangeDecodeError::none) {
        return true;
    }
    result.error = LzssContextualRangeDecodeError::entropy_error;
    return false;
}

template<class Decoder>
[[nodiscard]] bool read_bypass(
    Decoder& decoder, const std::uint8_t bits,
    std::uint32_t& value,
    LzssContextualRangeDecodeResult& result) noexcept {
    result.entropy = decoder.decode_bypass(bits, value);
    if (result.entropy.error == ContextualDynamicRangeDecodeError::none) {
        return true;
    }
    result.error = LzssContextualRangeDecodeError::entropy_error;
    return false;
}

// Context 9 determines its interval family from its own grammar cursor.
// Check agreement with the shared token walker before publishing the value.
[[nodiscard]] bool read_symbol(
    entropy::internal::LzssPositionDistanceRangeDecoder& decoder,
    const std::uint16_t context_id, const std::uint16_t alphabet,
    std::uint32_t& value, LzssContextualRangeDecodeResult& result) noexcept {
    ModeledOperation operation{};
    result.entropy = decoder.decode_next(operation);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
        return false;
    }
    if (operation.kind != ModeledOperationKind::symbol
        || operation.context_id != context_id || operation.alphabet_size != alphabet) {
        result.error = LzssContextualRangeDecodeError::internal_error;
        return false;
    }
    value = operation.value;
    return true;
}

[[nodiscard]] bool read_bypass(
    entropy::internal::LzssPositionDistanceRangeDecoder& decoder,
    const std::uint8_t bits, std::uint32_t& value,
    LzssContextualRangeDecodeResult& result) noexcept {
    ModeledOperation operation{};
    result.entropy = decoder.decode_next(operation);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
        return false;
    }
    if (operation.kind != ModeledOperationKind::bypass_bits || operation.bit_count != bits) {
        result.error = LzssContextualRangeDecodeError::internal_error;
        return false;
    }
    value = operation.value;
    return true;
}

template<class Decoder>
[[nodiscard]] bool read_token(
    Decoder& decoder, const LzssFieldContextState& state,
    const LengthMapping mapping,
    LzssTypedToken& token,
    LzssContextualRangeDecodeResult& result) noexcept {
    const auto context_id = [mapping](const std::uint16_t id) {
        return static_cast<std::uint16_t>(
            mapping != LengthMapping::reduced_literal || id < 4 ? id
            : id < 20 ? 4 + (id - 4) / 2 : id - 8);
    };
    std::uint32_t kind{};
    if (!read_symbol(decoder, state.token_context(), 2, kind, result)) {
        return false;
    }
    if (kind == 0) {
        std::uint32_t literal{};
        if (!read_symbol(decoder, context_id(state.literal_context()), 256, literal,
                         result)) {
            return false;
        }
        token = {LzssTypedTokenKind::literal,
                 static_cast<std::uint8_t>(literal), 0, 0};
        return true;
    }

    std::uint32_t length_class{};
    if (!read_symbol(decoder, context_id(state.length_context()), 9, length_class,
                     result)) {
        return false;
    }
    std::uint32_t length_extra{};
    const auto length_bits = static_cast<std::uint8_t>(
        mapping != LengthMapping::shifted_two && length_class == 8
            ? 1 : length_class);
    if (length_bits != 0
        && !read_bypass(decoder, length_bits,
                        length_extra, result)) {
        return false;
    }
    std::uint32_t length{};
    if (mapping != LengthMapping::shifted_two) {
        const auto decoded = decode_lzss_short_length_escape(
            length_class, length_bits, length_extra);
        if (decoded.error != LzssShortLengthEscapeError::none) {
            result.error = LzssContextualRangeDecodeError::invalid_token;
            return false;
        }
        length = decoded.length;
    } else {
        length = (UINT32_C(1) << length_class) + length_extra + 2;
    }
    const auto distance_context =
        LzssFieldContextState::distance_context(length_class);
    std::uint32_t distance_class{};
    if (!read_symbol(decoder, context_id(distance_context), 17, distance_class, result)) {
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
    LzssContextualRangeDecodeResult& result) noexcept {
    const auto tokens = static_cast<std::uint64_t>(
        context.declared_token_count);
    const auto events = static_cast<std::uint64_t>(
        context.declared_event_count);
    const auto decisions = static_cast<std::uint64_t>(
        context.declared_decision_count);
    const auto raw = static_cast<std::uint64_t>(context.declared_raw_size);
    if (raw == 0 || raw > 65536 || tokens == 0 || tokens > raw
        || events < 2 * tokens || events > 5 * tokens || events > 2 * raw
        || decisions < events || decisions > 27 * tokens
        || decisions > 9 * raw) {
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
        || raw > limits.max_frame_size || raw > limits.max_block_size) {
        result.error = LzssContextualRangeDecodeError::limit_exceeded;
        return false;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(context.output_already_committed, raw,
                           total_output)) {
        result.error = LzssContextualRangeDecodeError::arithmetic_overflow;
        return false;
    }
    if (total_output > limits.max_total_output_size) {
        result.error = LzssContextualRangeDecodeError::limit_exceeded;
        return false;
    }
    return true;
}

template<class Decoder = LzssShortMatchRangeDecoder>
[[nodiscard]] LzssContextualRangeDecodeResult run_pass(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const LengthMapping mapping,
    const std::span<LzssTypedToken> output) noexcept {
    LzssContextualRangeDecodeResult result{};
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits, token_variant(mapping));
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssContextualRangeDecodeError::limit_exceeded
            : LzssContextualRangeDecodeError::invalid_parameters;
        return result;
    }
    if (!validate_declared_bounds(context, limits, result)) return result;
    if (descriptor.decision_count != context.declared_decision_count) {
        result.error = LzssContextualRangeDecodeError::invalid_counts;
        return result;
    }
    const auto raw = static_cast<std::uint64_t>(context.declared_raw_size);
    const auto decisions = static_cast<std::uint64_t>(
        context.declared_decision_count);
    if (descriptor.payload_size > 18 * raw + 5
        || descriptor.payload_size > 2 * decisions + 5) {
        result.error = LzssContextualRangeDecodeError::invalid_counts;
        return result;
    }

    if (mapping == LengthMapping::reduced_literal) {
        std::size_t token_bytes{};
        std::size_t aggregate{};
        if (!core::checked_multiply(static_cast<std::size_t>(context.declared_token_count),
                                    sizeof(LzssTypedToken), token_bytes)
            || !core::checked_add(token_bytes, payload.size(), aggregate)
            || !core::checked_add(aggregate, sizeof(Decoder), aggregate)) {
            result.error = LzssContextualRangeDecodeError::arithmetic_overflow;
            return result;
        }
        if (aggregate > limits.max_internal_buffered_bytes) {
            result.error = LzssContextualRangeDecodeError::limit_exceeded;
            return result;
        }
    }
    Decoder decoder;
    result.entropy = decoder.begin(descriptor, payload, limits);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
        return result;
    }
    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        LzssTypedToken token{};
        if (!read_token(decoder, state, mapping, token, result)) return result;
        std::uint64_t next_raw{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters,
            {result.raw_size, context.declared_raw_size}, limits, next_raw,
            token_variant(mapping));
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
        result.raw_size = next_raw;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.entropy = decoder.finish(context.declared_event_count,
                                    context.declared_decision_count);
    if (result.entropy.error != ContextualDynamicRangeDecodeError::none) {
        result.error = LzssContextualRangeDecodeError::entropy_error;
    } else if (result.raw_size != context.declared_raw_size) {
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
    const auto token_begin =
        reinterpret_cast<std::uintptr_t>(tokens.data());
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

LzssContextualRangeDecodeResult validate_lzss_short_match_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_pass(descriptor, payload, parameters, context, limits,
                    LengthMapping::shifted_two, {});
}

namespace {

template<class Decoder = LzssShortMatchRangeDecoder>
[[nodiscard]] LzssContextualRangeDecodeResult decode_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens,
    const LengthMapping mapping) noexcept {
    auto result = run_pass<Decoder>(descriptor, payload, parameters, context, limits,
                           mapping, {});
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
    const auto decoded = run_pass<Decoder>(descriptor, payload, parameters, context,
                                  limits, mapping, output);
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

} // namespace

LzssContextualRangeDecodeResult decode_lzss_short_match_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    return decode_tokens(descriptor, payload, parameters, context, limits,
                         private_tokens, LengthMapping::shifted_two);
}

LzssContextualRangeDecodeResult
validate_lzss_short_length_escape_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_pass(descriptor, payload, parameters, context, limits,
                    LengthMapping::isolated_escape, {});
}

LzssContextualRangeDecodeResult decode_lzss_short_length_escape_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    return decode_tokens(descriptor, payload, parameters, context, limits,
                         private_tokens, LengthMapping::isolated_escape);
}

LzssContextualRangeDecodeResult
validate_lzss_reduced_literal_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_pass<entropy::internal::LzssReducedLiteralRangeDecoder>(descriptor, payload, parameters, context, limits,
                    LengthMapping::reduced_literal, {});
}

LzssContextualRangeDecodeResult decode_lzss_reduced_literal_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    return decode_tokens<entropy::internal::LzssReducedLiteralRangeDecoder>(descriptor, payload, parameters, context, limits,
                         private_tokens, LengthMapping::reduced_literal);
}

LzssContextualRangeDecodeResult validate_lzss_position_distance_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_pass<entropy::internal::LzssPositionDistanceRangeDecoder>(
        descriptor, payload, parameters, context, limits, LengthMapping::reduced_literal, {});
}

LzssContextualRangeDecodeResult decode_lzss_position_distance_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    return decode_tokens<entropy::internal::LzssPositionDistanceRangeDecoder>(
        descriptor, payload, parameters, context, limits, private_tokens, LengthMapping::reduced_literal);
}

} // namespace marc::context::internal

#include "dictionary/lzss_typed_token.hpp"

#include "core/checked_math.hpp"

namespace marc::dictionary::internal {

LzssTypedTokenError validate_lzss_typed_parameters(
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssTypedTokenError::limit_exceeded;
    }
    const auto format_error = validate_lzss_parameters(parameters, limits);
    if (format_error == LzssFormatError::limit_exceeded) {
        return LzssTypedTokenError::limit_exceeded;
    }
    std::uint32_t maximum_window{};
    switch (variant) {
    case LzssTypedTokenVariant::field_context_64k:
        maximum_window = 65536;
        break;
    case LzssTypedTokenVariant::field_context_1m:
        maximum_window = 1048576;
        break;
    default:
        return LzssTypedTokenError::invalid_parameters;
    }
    if (format_error != LzssFormatError::none
        || parameters.min_match_length != 5
        || parameters.max_match_length > 258
        || parameters.window_size > maximum_window) {
        return LzssTypedTokenError::invalid_parameters;
    }
    return LzssTypedTokenError::none;
}

LzssTypedTokenError validate_lzss_typed_token(
    const LzssTypedToken& token,
    const LzssParameters& parameters,
    const LzssTypedTokenValidationContext& context,
    const core::DecoderLimits& limits,
    std::uint64_t& next_raw_size,
    const LzssTypedTokenVariant variant) noexcept {
    const auto parameter_error =
        validate_lzss_typed_parameters(parameters, limits, variant);
    if (parameter_error != LzssTypedTokenError::none) {
        return parameter_error;
    }
    if (context.declared_raw_size > limits.max_frame_size
        || context.raw_already_produced > context.declared_raw_size) {
        return LzssTypedTokenError::limit_exceeded;
    }

    const auto raw_kind = static_cast<std::uint8_t>(token.kind);
    if (raw_kind > static_cast<std::uint8_t>(LzssTypedTokenKind::match)) {
        return LzssTypedTokenError::unknown_kind;
    }

    std::uint64_t produced = context.raw_already_produced;
    if (token.kind == LzssTypedTokenKind::literal) {
        if (token.distance != 0 || token.length != 0) {
            return LzssTypedTokenError::nonzero_unused_field;
        }
        if (!core::checked_add(produced, UINT64_C(1), produced)) {
            return LzssTypedTokenError::arithmetic_overflow;
        }
    } else {
        if (token.literal != 0) {
            return LzssTypedTokenError::nonzero_unused_field;
        }
        if (token.distance == 0 || token.distance > parameters.window_size
            || token.distance > context.raw_already_produced) {
            return LzssTypedTokenError::invalid_distance;
        }
        if (token.length < parameters.min_match_length
            || token.length > parameters.max_match_length) {
            return LzssTypedTokenError::invalid_length;
        }
        if (!core::checked_add(
                produced, static_cast<std::uint64_t>(token.length), produced)) {
            return LzssTypedTokenError::arithmetic_overflow;
        }
    }
    if (produced > context.declared_raw_size) {
        return LzssTypedTokenError::output_size_mismatch;
    }
    next_raw_size = produced;
    return LzssTypedTokenError::none;
}

LzssTypedFrameValidationResult validate_lzss_typed_frame(
    const std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    LzssTypedFrameValidationResult result{};
    const auto parameter_error =
        validate_lzss_typed_parameters(parameters, limits, variant);
    if (parameter_error != LzssTypedTokenError::none) {
        result.token_error = parameter_error;
        result.error = parameter_error == LzssTypedTokenError::limit_exceeded
            ? LzssTypedFrameValidationError::limit_exceeded
            : LzssTypedFrameValidationError::invalid_parameters;
        return result;
    }
    if (tokens.size() != context.declared_token_count) {
        result.error = LzssTypedFrameValidationError::token_count_mismatch;
        return result;
    }
    if (context.declared_raw_size > limits.max_frame_size
        || context.declared_raw_size > limits.max_block_size
        || context.output_already_committed > limits.max_total_output_size) {
        result.error = LzssTypedFrameValidationError::limit_exceeded;
        return result;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(context.output_already_committed,
                           static_cast<std::uint64_t>(context.declared_raw_size),
                           total_output)) {
        result.error = LzssTypedFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (total_output > limits.max_total_output_size) {
        result.error = LzssTypedFrameValidationError::limit_exceeded;
        return result;
    }
    std::size_t token_storage_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_storage_bytes)) {
        result.error = LzssTypedFrameValidationError::arithmetic_overflow;
        return result;
    }
    if (token_storage_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssTypedFrameValidationError::limit_exceeded;
        return result;
    }

    for (const auto& token : tokens) {
        result.token_index = result.token_count;
        if (result.raw_size == context.declared_raw_size) {
            result.error = LzssTypedFrameValidationError::trailing_tokens;
            return result;
        }
        std::uint64_t next_raw_size{};
        result.token_error = validate_lzss_typed_token(
            token, parameters,
            {result.raw_size, context.declared_raw_size}, limits,
            next_raw_size, variant);
        if (result.token_error != LzssTypedTokenError::none) {
            if (result.token_error
                == LzssTypedTokenError::arithmetic_overflow) {
                result.error =
                    LzssTypedFrameValidationError::arithmetic_overflow;
            } else if (result.token_error
                       == LzssTypedTokenError::limit_exceeded) {
                result.error = LzssTypedFrameValidationError::limit_exceeded;
            } else {
                result.error = LzssTypedFrameValidationError::token_error;
            }
            return result;
        }
        result.raw_size = next_raw_size;
        ++result.token_count;
    }
    result.token_index = result.token_count;
    if (result.raw_size != context.declared_raw_size) {
        result.error = LzssTypedFrameValidationError::premature_end;
    }
    return result;
}

} // namespace marc::dictionary::internal

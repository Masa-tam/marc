#include "context/lzss_short_length_escape_operation_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::
    field_context_64k_short_length_escape;

[[nodiscard]] bool add_decisions(const std::uint32_t count,
                                 LzssFieldContextResult& result) noexcept {
    if (!core::checked_add(result.decision_count, count,
                           result.decision_count)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return false;
    }
    return true;
}

[[nodiscard]] bool read_symbol(const std::span<const ModeledOperation> operations,
                               const std::uint16_t context,
                               const std::uint16_t alphabet,
                               std::uint32_t& value,
                               LzssFieldContextResult& result) noexcept {
    result.operation_index = result.operation_count;
    if (result.operation_count == operations.size()) {
        result.error = LzssFieldContextError::truncated_token;
        return false;
    }
    const auto& operation = operations[result.operation_count];
    if (operation.kind != ModeledOperationKind::symbol) {
        result.error = LzssFieldContextError::unexpected_operation_kind;
    } else if (operation.bit_count != 0) {
        result.error = LzssFieldContextError::nonzero_unused_field;
    } else if (operation.context_id != context) {
        result.error = LzssFieldContextError::unexpected_context;
    } else if (operation.alphabet_size != alphabet) {
        result.error = LzssFieldContextError::unexpected_alphabet;
    } else if (operation.value >= alphabet) {
        result.error = LzssFieldContextError::invalid_symbol;
    }
    if (result.error != LzssFieldContextError::none) return false;
    value = operation.value;
    ++result.operation_count;
    return add_decisions(1, result);
}

[[nodiscard]] bool read_bypass(const std::span<const ModeledOperation> operations,
                               const std::uint8_t bits,
                               std::uint32_t& value,
                               LzssFieldContextResult& result) noexcept {
    result.operation_index = result.operation_count;
    if (result.operation_count == operations.size()) {
        result.error = LzssFieldContextError::truncated_token;
        return false;
    }
    const auto& operation = operations[result.operation_count];
    if (operation.kind != ModeledOperationKind::bypass_bits) {
        result.error = LzssFieldContextError::unexpected_operation_kind;
    } else if (operation.context_id != 0 || operation.alphabet_size != 0) {
        result.error = LzssFieldContextError::nonzero_unused_field;
    } else if (operation.bit_count != bits || bits == 0 || bits > 16) {
        result.error = LzssFieldContextError::invalid_bypass_width;
    } else if (operation.value >= (UINT32_C(1) << bits)) {
        result.error = LzssFieldContextError::invalid_symbol;
    }
    if (result.error != LzssFieldContextError::none) return false;
    value = operation.value;
    ++result.operation_count;
    return add_decisions(bits, result);
}

[[nodiscard]] bool read_token(const std::span<const ModeledOperation> operations,
                              const LzssFieldContextState& state,
                              LzssTypedToken& token,
                              LzssFieldContextResult& result) noexcept {
    std::uint32_t kind{};
    if (!read_symbol(operations, state.token_context(), 2, kind, result)) {
        return false;
    }
    if (kind == 0) {
        std::uint32_t literal{};
        if (!read_symbol(operations, state.literal_context(), 256,
                         literal, result)) return false;
        token = {LzssTypedTokenKind::literal,
                 static_cast<std::uint8_t>(literal), 0, 0};
        return true;
    }

    std::uint32_t length_class{};
    if (!read_symbol(operations, state.length_context(), 9,
                     length_class, result)) return false;
    const auto length_bits = static_cast<std::uint8_t>(
        length_class == 8 ? 1 : length_class);
    std::uint32_t length_extra{};
    if (length_bits != 0
        && !read_bypass(operations, length_bits, length_extra, result)) {
        return false;
    }
    const auto length = decode_lzss_short_length_escape(
        length_class, length_bits, length_extra);
    if (length.error != LzssShortLengthEscapeError::none) {
        result.error = LzssFieldContextError::invalid_token;
        return false;
    }

    std::uint32_t distance_class{};
    if (!read_symbol(operations,
                     LzssFieldContextState::distance_context(length_class),
                     17, distance_class, result)) return false;
    std::uint32_t distance_extra{};
    if (distance_class != 0
        && !read_bypass(operations,
                        static_cast<std::uint8_t>(distance_class),
                        distance_extra, result)) return false;
    token = {LzssTypedTokenKind::match, 0,
             (UINT32_C(1) << distance_class) + distance_extra,
             length.length};
    return true;
}

[[nodiscard]] LzssFieldContextResult run(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> output) noexcept {
    LzssFieldContextResult result{};
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits, token_variant);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssFieldContextError::limit_exceeded
            : LzssFieldContextError::invalid_parameters;
        return result;
    }
    if (operations.size() != context.declared_event_count) {
        result.error = LzssFieldContextError::event_count_mismatch;
        return result;
    }
    const auto tokens = static_cast<std::uint64_t>(context.declared_token_count);
    const auto events = static_cast<std::uint64_t>(context.declared_event_count);
    const auto decisions = static_cast<std::uint64_t>(
        context.declared_decision_count);
    const auto raw = static_cast<std::uint64_t>(context.declared_raw_size);
    if (tokens > raw) {
        result.error = LzssFieldContextError::token_count_mismatch;
        return result;
    }
    if (events < 2 * tokens || events > 5 * tokens || events > 2 * raw) {
        result.error = LzssFieldContextError::event_count_mismatch;
        return result;
    }
    if (decisions < events || decisions > 27 * tokens
        || decisions > 9 * raw) {
        result.error = LzssFieldContextError::decision_count_mismatch;
        return result;
    }
    std::size_t operation_bytes{};
    std::size_t token_bytes{};
    std::size_t combined_bytes{};
    if (!core::checked_multiply(operations.size(), sizeof(ModeledOperation),
                                operation_bytes)
        || !core::checked_multiply(static_cast<std::size_t>(tokens),
                                   sizeof(LzssTypedToken), token_bytes)
        || !core::checked_add(operation_bytes, token_bytes, combined_bytes)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return result;
    }
    if (combined_bytes > limits.max_internal_buffered_bytes
        || raw > limits.max_frame_size || raw > limits.max_block_size
        || raw > 65536) {
        result.error = LzssFieldContextError::limit_exceeded;
        return result;
    }

    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        if (result.raw_size == raw) {
            result.error = LzssFieldContextError::trailing_tokens;
            return result;
        }
        LzssTypedToken token{};
        if (!read_token(operations, state, token, result)) return result;
        std::uint64_t next_raw{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters, {result.raw_size, raw}, limits,
            next_raw, token_variant);
        if (result.token_error != LzssTypedTokenError::none) {
            result.error = result.token_error == LzssTypedTokenError::limit_exceeded
                ? LzssFieldContextError::limit_exceeded
                : result.token_error == LzssTypedTokenError::arithmetic_overflow
                    ? LzssFieldContextError::arithmetic_overflow
                    : LzssFieldContextError::invalid_token;
            return result;
        }
        if (!output.empty()) output[result.token_count] = token;
        result.raw_size = next_raw;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.operation_index = result.operation_count;
    if (result.operation_count != operations.size()) {
        result.error = LzssFieldContextError::trailing_operations;
    } else if (result.decision_count != context.declared_decision_count) {
        result.error = LzssFieldContextError::decision_count_mismatch;
    } else if (result.raw_size != raw) {
        result.error = LzssFieldContextError::raw_size_mismatch;
    } else {
        std::uint64_t total_output{};
        if (!core::checked_add(context.output_already_committed,
                               result.raw_size, total_output)) {
            result.error = LzssFieldContextError::arithmetic_overflow;
        } else if (total_output > limits.max_total_output_size) {
            result.error = LzssFieldContextError::limit_exceeded;
        }
    }
    return result;
}

[[nodiscard]] LzssFieldContextError check_overlap(
    const std::span<const ModeledOperation> operations,
    const std::span<LzssTypedToken> output) noexcept {
    if (operations.empty() || output.empty()) return LzssFieldContextError::none;
    std::size_t operation_bytes{};
    std::size_t output_bytes{};
    if (!core::checked_multiply(operations.size(), sizeof(ModeledOperation),
                                operation_bytes)
        || !core::checked_multiply(output.size(), sizeof(LzssTypedToken),
                                   output_bytes)) {
        return LzssFieldContextError::arithmetic_overflow;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(operations.data());
    const auto output_begin = reinterpret_cast<std::uintptr_t>(output.data());
    std::uintptr_t end{};
    std::uintptr_t output_end{};
    if (!core::checked_add(begin, static_cast<std::uintptr_t>(operation_bytes),
                           end)
        || !core::checked_add(output_begin,
                              static_cast<std::uintptr_t>(output_bytes),
                              output_end)) {
        return LzssFieldContextError::arithmetic_overflow;
    }
    return begin < output_end && output_begin < end
        ? LzssFieldContextError::overlapping_buffers
        : LzssFieldContextError::none;
}

} // namespace

LzssFieldContextResult validate_lzss_short_length_escape_operations(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run(operations, parameters, context, limits, {});
}

LzssFieldContextResult invert_lzss_short_length_escape_operations(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<LzssTypedToken> private_tokens) noexcept {
    auto result = run(operations, parameters, context, limits, {});
    if (result.error != LzssFieldContextError::none) return result;
    if (private_tokens.size() < context.declared_token_count) {
        result.error = LzssFieldContextError::output_too_small;
        return result;
    }
    const auto output = private_tokens.first(context.declared_token_count);
    result.error = check_overlap(operations, output);
    if (result.error != LzssFieldContextError::none) return result;
    return run(operations, parameters, context, limits, output);
}

} // namespace marc::context::internal

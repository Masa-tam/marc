#include "context/lzss_field_context.hpp"
#include "context/lzss_field_context_state.hpp"

#include "core/checked_math.hpp"

#include <bit>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] bool add_decisions(const std::uint32_t increment,
                                 LzssFieldContextResult& result) noexcept {
    std::uint32_t updated{};
    if (!core::checked_add(result.decision_count, increment, updated)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return false;
    }
    result.decision_count = updated;
    return true;
}

[[nodiscard]] bool read_symbol(
    const std::span<const ModeledOperation> operations,
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet,
    std::uint32_t& value,
    LzssFieldContextResult& result) noexcept {
    result.operation_index = result.operation_count;
    if (result.operation_count >= operations.size()) {
        result.error = LzssFieldContextError::truncated_token;
        return false;
    }
    const auto& operation = operations[result.operation_count];
    if (operation.kind != ModeledOperationKind::symbol) {
        result.error = LzssFieldContextError::unexpected_operation_kind;
        return false;
    }
    if (operation.bit_count != 0) {
        result.error = LzssFieldContextError::nonzero_unused_field;
        return false;
    }
    if (operation.context_id != expected_context) {
        result.error = LzssFieldContextError::unexpected_context;
        return false;
    }
    if (operation.alphabet_size != expected_alphabet) {
        result.error = LzssFieldContextError::unexpected_alphabet;
        return false;
    }
    if (operation.value >= expected_alphabet) {
        result.error = LzssFieldContextError::invalid_symbol;
        return false;
    }
    value = operation.value;
    ++result.operation_count;
    return add_decisions(1, result);
}

[[nodiscard]] bool read_bypass(
    const std::span<const ModeledOperation> operations,
    const std::uint8_t expected_bits,
    std::uint32_t& value,
    LzssFieldContextResult& result) noexcept {
    result.operation_index = result.operation_count;
    if (result.operation_count >= operations.size()) {
        result.error = LzssFieldContextError::truncated_token;
        return false;
    }
    const auto& operation = operations[result.operation_count];
    if (operation.kind != ModeledOperationKind::bypass_bits) {
        result.error = LzssFieldContextError::unexpected_operation_kind;
        return false;
    }
    if (operation.context_id != 0 || operation.alphabet_size != 0) {
        result.error = LzssFieldContextError::nonzero_unused_field;
        return false;
    }
    if (operation.bit_count != expected_bits || expected_bits == 0
        || expected_bits > 16) {
        result.error = LzssFieldContextError::invalid_bypass_width;
        return false;
    }
    if (operation.value >= (UINT32_C(1) << expected_bits)) {
        result.error = LzssFieldContextError::invalid_symbol;
        return false;
    }
    value = operation.value;
    ++result.operation_count;
    return add_decisions(expected_bits, result);
}

[[nodiscard]] bool read_token(
    const std::span<const ModeledOperation> operations,
    const LzssFieldContextState& state,
    LzssTypedToken& token,
    LzssFieldContextResult& result) noexcept {
    std::uint32_t kind{};
    if (!read_symbol(operations, state.token_context(), 2, kind, result)) {
        return false;
    }
    if (kind == 0) {
        std::uint32_t literal{};
        if (!read_symbol(
                operations, state.literal_context(), 256, literal, result)) {
            return false;
        }
        token = {LzssTypedTokenKind::literal,
                 static_cast<std::uint8_t>(literal), 0, 0};
        return true;
    }

    std::uint32_t length_class{};
    if (!read_symbol(operations,
                     state.length_context(), 8,
                     length_class, result)) {
        return false;
    }
    std::uint32_t length_extra{};
    if (length_class != 0
        && !read_bypass(operations, static_cast<std::uint8_t>(length_class),
                        length_extra, result)) {
        return false;
    }
    const auto length_value =
        (UINT32_C(1) << length_class) + length_extra;
    const auto length = length_value + 4;

    std::uint32_t distance_class{};
    if (!read_symbol(
            operations, LzssFieldContextState::distance_context(length_class),
            17,
            distance_class, result)) {
        return false;
    }
    std::uint32_t distance_extra{};
    if (distance_class != 0
        && !read_bypass(operations, static_cast<std::uint8_t>(distance_class),
                        distance_extra, result)) {
        return false;
    }
    const auto distance =
        (UINT32_C(1) << distance_class) + distance_extra;
    token = {LzssTypedTokenKind::match, 0, distance, length};
    return true;
}

[[nodiscard]] std::uint8_t value_class(
    const std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

[[nodiscard]] LzssFieldContextError map_typed_frame_error(
    const dictionary::internal::LzssTypedFrameValidationError error) noexcept {
    using Error = dictionary::internal::LzssTypedFrameValidationError;
    switch (error) {
    case Error::none:
        return LzssFieldContextError::none;
    case Error::invalid_parameters:
        return LzssFieldContextError::invalid_parameters;
    case Error::token_count_mismatch:
        return LzssFieldContextError::token_count_mismatch;
    case Error::token_error:
        return LzssFieldContextError::invalid_token;
    case Error::premature_end:
        return LzssFieldContextError::raw_size_mismatch;
    case Error::trailing_tokens:
        return LzssFieldContextError::trailing_tokens;
    case Error::limit_exceeded:
        return LzssFieldContextError::limit_exceeded;
    case Error::arithmetic_overflow:
        return LzssFieldContextError::arithmetic_overflow;
    }
    return LzssFieldContextError::invalid_token;
}

[[nodiscard]] LzssFieldContextResult run_plan(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    LzssFieldContextResult result{};
    const auto validation = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits);
    result.token_count = validation.token_count;
    result.token_index = validation.token_index;
    result.raw_size = validation.raw_size;
    result.token_error = validation.token_error;
    result.error = map_typed_frame_error(validation.error);
    if (result.error != LzssFieldContextError::none) return result;

    for (const auto& token : tokens) {
        std::size_t event_increment{2};
        std::uint32_t decision_increment{2};
        if (token.kind == LzssTypedTokenKind::match) {
            const auto length_class = value_class(token.length - 4);
            const auto distance_class = value_class(token.distance);
            event_increment = static_cast<std::size_t>(
                3U + (length_class != 0 ? 1U : 0U)
                + (distance_class != 0 ? 1U : 0U));
            decision_increment = static_cast<std::uint32_t>(
                3U + length_class + distance_class);
        }
        if (!core::checked_add(result.operation_count, event_increment,
                               result.operation_count)
            || !core::checked_add(result.decision_count, decision_increment,
                                  result.decision_count)) {
            result.error = LzssFieldContextError::arithmetic_overflow;
            return result;
        }
    }
    result.operation_index = result.operation_count;
    std::size_t operation_bytes{};
    if (!core::checked_multiply(result.operation_count,
                                sizeof(ModeledOperation), operation_bytes)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
    } else if (operation_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssFieldContextError::limit_exceeded;
    }
    return result;
}

[[nodiscard]] LzssFieldContextResult run_validation(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    LzssFieldContextResult result{};
    const auto parameter_error =
        dictionary::internal::validate_lzss_typed_parameters(
            parameters, limits);
    if (parameter_error != LzssTypedTokenError::none) {
        result.token_error = parameter_error;
        result.error = parameter_error == LzssTypedTokenError::limit_exceeded
            ? LzssFieldContextError::limit_exceeded
            : LzssFieldContextError::invalid_parameters;
        return result;
    }
    if (operations.size() != context.declared_event_count) {
        result.error = LzssFieldContextError::event_count_mismatch;
        return result;
    }
    const auto token_count =
        static_cast<std::uint64_t>(context.declared_token_count);
    const auto event_count =
        static_cast<std::uint64_t>(context.declared_event_count);
    const auto decision_count =
        static_cast<std::uint64_t>(context.declared_decision_count);
    const auto raw_size =
        static_cast<std::uint64_t>(context.declared_raw_size);
    if (token_count > raw_size) {
        result.error = LzssFieldContextError::token_count_mismatch;
        return result;
    }
    if (event_count < 2 * token_count || event_count > 5 * token_count
        || event_count > 2 * raw_size) {
        result.error = LzssFieldContextError::event_count_mismatch;
        return result;
    }
    if (decision_count < event_count || decision_count > 26 * token_count
        || decision_count > 6 * raw_size) {
        result.error = LzssFieldContextError::decision_count_mismatch;
        return result;
    }
    std::size_t operation_bytes{};
    if (!core::checked_multiply(operations.size(), sizeof(ModeledOperation),
                                operation_bytes)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return result;
    }
    if (operation_bytes > limits.max_internal_buffered_bytes
        || context.declared_raw_size > limits.max_frame_size
        || context.declared_raw_size > limits.max_block_size) {
        result.error = LzssFieldContextError::limit_exceeded;
        return result;
    }

    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        LzssTypedToken token{};
        if (!read_token(operations, state, token, result)) return result;

        std::uint64_t next_raw_size{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters,
            {result.raw_size, context.declared_raw_size}, limits,
            next_raw_size);
        if (result.token_error != LzssTypedTokenError::none) {
            if (result.token_error
                == LzssTypedTokenError::arithmetic_overflow) {
                result.error = LzssFieldContextError::arithmetic_overflow;
            } else if (result.token_error
                       == LzssTypedTokenError::limit_exceeded) {
                result.error = LzssFieldContextError::limit_exceeded;
            } else {
                result.error = LzssFieldContextError::invalid_token;
            }
            return result;
        }
        result.raw_size = next_raw_size;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.operation_index = result.operation_count;
    if (result.operation_count != operations.size()) {
        result.error = LzssFieldContextError::trailing_operations;
    } else if (result.token_count != context.declared_token_count) {
        result.error = LzssFieldContextError::token_count_mismatch;
    } else if (result.decision_count != context.declared_decision_count) {
        result.error = LzssFieldContextError::decision_count_mismatch;
    } else if (result.raw_size != context.declared_raw_size) {
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

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck operation_token_overlap(
    const std::span<const ModeledOperation> operations,
    const std::span<LzssTypedToken> tokens) noexcept {
    if (operations.empty() || tokens.empty()) return OverlapCheck::disjoint;
    std::size_t operation_bytes{};
    std::size_t token_bytes{};
    if (!core::checked_multiply(operations.size(), sizeof(ModeledOperation),
                                operation_bytes)
        || !core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                   token_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    const auto token_begin = reinterpret_cast<std::uintptr_t>(tokens.data());
    std::uintptr_t operation_end{};
    std::uintptr_t token_end{};
    if (!core::checked_add(operation_begin,
                           static_cast<std::uintptr_t>(operation_bytes),
                           operation_end)
        || !core::checked_add(token_begin,
                              static_cast<std::uintptr_t>(token_bytes),
                              token_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return operation_begin < token_end && token_begin < operation_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

[[nodiscard]] OverlapCheck token_operation_overlap(
    const std::span<const LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations) noexcept {
    if (tokens.empty() || operations.empty()) return OverlapCheck::disjoint;
    std::size_t token_bytes{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)
        || !core::checked_multiply(operations.size(),
                                   sizeof(ModeledOperation),
                                   operation_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto token_begin = reinterpret_cast<std::uintptr_t>(tokens.data());
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    std::uintptr_t token_end{};
    std::uintptr_t operation_end{};
    if (!core::checked_add(token_begin,
                           static_cast<std::uintptr_t>(token_bytes), token_end)
        || !core::checked_add(operation_begin,
                              static_cast<std::uintptr_t>(operation_bytes),
                              operation_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return token_begin < operation_end && operation_begin < token_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

void materialize_operations(
    const std::span<const LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations) noexcept {
    LzssFieldContextState state{};
    std::size_t operation_index{};
    const auto write_symbol = [&](const std::uint16_t context,
                                  const std::uint16_t alphabet,
                                  const std::uint32_t value) {
        operations[operation_index++] = {
            ModeledOperationKind::symbol, context, alphabet, value, 0};
    };
    const auto write_bypass = [&](const std::uint8_t bits,
                                  const std::uint32_t value) {
        operations[operation_index++] = {
            ModeledOperationKind::bypass_bits, 0, 0, value, bits};
    };
    for (const auto& token : tokens) {
        if (token.kind == LzssTypedTokenKind::literal) {
            write_symbol(state.token_context(), 2, 0);
            write_symbol(state.literal_context(), 256, token.literal);
            state.accept(token);
            continue;
        }

        write_symbol(state.token_context(), 2, 1);
        const auto length_value = token.length - 4;
        const auto length_class = value_class(length_value);
        const auto length_base = UINT32_C(1) << length_class;
        write_symbol(state.length_context(), 8, length_class);
        if (length_class != 0) {
            write_bypass(length_class, length_value - length_base);
        }

        const auto distance_class = value_class(token.distance);
        const auto distance_base = UINT32_C(1) << distance_class;
        write_symbol(LzssFieldContextState::distance_context(length_class), 17,
                     distance_class);
        if (distance_class != 0) {
            write_bypass(distance_class, token.distance - distance_base);
        }
        state.accept(token);
    }
}

void materialize(const std::span<const ModeledOperation> operations,
                 const std::span<LzssTypedToken> tokens) noexcept {
    std::size_t operation_index{};
    for (auto& token : tokens) {
        const auto kind = operations[operation_index++].value;
        if (kind == 0) {
            token = {LzssTypedTokenKind::literal,
                     static_cast<std::uint8_t>(
                         operations[operation_index++].value),
                     0, 0};
            continue;
        }
        const auto length_class = operations[operation_index++].value;
        std::uint32_t length_extra{};
        if (length_class != 0) {
            length_extra = operations[operation_index++].value;
        }
        const auto distance_class = operations[operation_index++].value;
        std::uint32_t distance_extra{};
        if (distance_class != 0) {
            distance_extra = operations[operation_index++].value;
        }
        token = {
            LzssTypedTokenKind::match,
            0,
            (UINT32_C(1) << distance_class) + distance_extra,
            (UINT32_C(1) << length_class) + length_extra + 4};
    }
}

} // namespace

LzssFieldContextResult plan_lzss_field_context_operations(
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_plan(tokens, parameters, context, limits);
}

LzssFieldContextResult model_lzss_field_context_tokens(
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<ModeledOperation> private_operations) noexcept {
    auto result = run_plan(tokens, parameters, context, limits);
    if (result.error != LzssFieldContextError::none) return result;
    if (private_operations.size() < result.operation_count) {
        result.error = LzssFieldContextError::output_too_small;
        return result;
    }
    const auto output = private_operations.first(result.operation_count);
    const auto overlap = token_operation_overlap(tokens, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return result;
    }
    if (overlap == OverlapCheck::overlap) {
        result.error = LzssFieldContextError::overlapping_buffers;
        return result;
    }
    materialize_operations(tokens, output);
    return result;
}

LzssFieldContextResult validate_lzss_field_context_operations(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return run_validation(operations, parameters, context, limits);
}

LzssFieldContextResult invert_lzss_field_context_operations(
    const std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    auto result = run_validation(operations, parameters, context, limits);
    if (result.error != LzssFieldContextError::none) return result;
    if (private_tokens.size() < context.declared_token_count) {
        result.error = LzssFieldContextError::output_too_small;
        return result;
    }
    const auto output = private_tokens.first(context.declared_token_count);
    const auto overlap = operation_token_overlap(operations, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return result;
    }
    if (overlap == OverlapCheck::overlap) {
        result.error = LzssFieldContextError::overlapping_buffers;
        return result;
    }
    materialize(operations, output);
    return result;
}

} // namespace marc::context::internal

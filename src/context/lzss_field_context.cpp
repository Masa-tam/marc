#include "context/lzss_field_context.hpp"

#include "core/checked_math.hpp"

#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;

enum class PreviousToken : std::uint8_t {
    start = 0,
    literal = 1,
    match = 2,
};

struct ModelState {
    PreviousToken previous{PreviousToken::start};
    bool has_literal{};
    std::uint8_t literal{};
};

[[nodiscard]] std::uint16_t state_index(const ModelState& state) noexcept {
    return static_cast<std::uint16_t>(state.previous);
}

[[nodiscard]] std::uint16_t literal_context(
    const ModelState& state) noexcept {
    if (!state.has_literal) return 3;
    return static_cast<std::uint16_t>(4 + (state.literal >> 4));
}

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
    const ModelState& state,
    LzssTypedToken& token,
    LzssFieldContextResult& result) noexcept {
    std::uint32_t kind{};
    if (!read_symbol(operations, state_index(state), 2, kind, result)) {
        return false;
    }
    if (kind == 0) {
        std::uint32_t literal{};
        if (!read_symbol(
                operations, literal_context(state), 256, literal, result)) {
            return false;
        }
        token = {LzssTypedTokenKind::literal,
                 static_cast<std::uint8_t>(literal), 0, 0};
        return true;
    }

    std::uint32_t length_class{};
    if (!read_symbol(operations,
                     static_cast<std::uint16_t>(20 + state_index(state)), 8,
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
            operations, static_cast<std::uint16_t>(23 + length_class), 17,
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

void update_state(const LzssTypedToken& token, ModelState& state) noexcept {
    if (token.kind == LzssTypedTokenKind::literal) {
        state.previous = PreviousToken::literal;
        state.has_literal = true;
        state.literal = token.literal;
    } else {
        state.previous = PreviousToken::match;
    }
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

    ModelState state{};
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
        update_state(token, state);
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

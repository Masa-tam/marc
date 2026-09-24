#include "context/lzss_short_match_operations.hpp"

#include "context/lzss_field_context_state.hpp"
#include "context/lzss_short_match_context_layout.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedFrameValidationError;
using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenKind;
using dictionary::internal::LzssTypedTokenVariant;

[[nodiscard]] LzssFieldContextError map_frame_error(
    const LzssTypedFrameValidationError error) noexcept {
    switch (error) {
    case LzssTypedFrameValidationError::none:
        return LzssFieldContextError::none;
    case LzssTypedFrameValidationError::invalid_parameters:
        return LzssFieldContextError::invalid_parameters;
    case LzssTypedFrameValidationError::token_count_mismatch:
        return LzssFieldContextError::token_count_mismatch;
    case LzssTypedFrameValidationError::token_error:
        return LzssFieldContextError::invalid_token;
    case LzssTypedFrameValidationError::premature_end:
        return LzssFieldContextError::raw_size_mismatch;
    case LzssTypedFrameValidationError::trailing_tokens:
        return LzssFieldContextError::trailing_tokens;
    case LzssTypedFrameValidationError::limit_exceeded:
        return LzssFieldContextError::limit_exceeded;
    case LzssTypedFrameValidationError::arithmetic_overflow:
        return LzssFieldContextError::arithmetic_overflow;
    }
    return LzssFieldContextError::invalid_token;
}

[[nodiscard]] bool overlaps(
    const std::span<const LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations,
    bool& overflow) noexcept {
    overflow = false;
    if (tokens.empty() || operations.empty()) return false;
    std::size_t token_bytes{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)
        || !core::checked_multiply(operations.size(),
                                   sizeof(ModeledOperation),
                                   operation_bytes)) {
        overflow = true;
        return false;
    }
    const auto token_begin =
        reinterpret_cast<std::uintptr_t>(tokens.data());
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    std::uintptr_t token_end{};
    std::uintptr_t operation_end{};
    if (!core::checked_add(token_begin,
                           static_cast<std::uintptr_t>(token_bytes), token_end)
        || !core::checked_add(operation_begin,
                              static_cast<std::uintptr_t>(operation_bytes),
                              operation_end)) {
        overflow = true;
        return false;
    }
    return token_begin < operation_end && operation_begin < token_end;
}

void write_symbol(const std::span<ModeledOperation> output,
                  std::size_t& index, const std::uint16_t context,
                  const std::uint32_t value) noexcept {
    output[index++] = {ModeledOperationKind::symbol, context,
                       lzss_short_match_alphabets[context], value, 0};
}

void write_bypass(const std::span<ModeledOperation> output,
                  std::size_t& index, const std::uint8_t width,
                  const std::uint32_t value) noexcept {
    output[index++] = {ModeledOperationKind::bypass_bits, 0, 0, value, width};
}

} // namespace

LzssFieldContextResult plan_lzss_short_match_operations(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    LzssFieldContextResult result{};
    const auto checked = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits,
        LzssTypedTokenVariant::field_context_64k_short_match);
    result.token_count = checked.token_count;
    result.token_index = checked.token_index;
    result.raw_size = checked.raw_size;
    result.token_error = checked.token_error;
    result.error = map_frame_error(checked.error);
    if (result.error != LzssFieldContextError::none) return result;

    for (const auto& token : tokens) {
        std::size_t events = 2;
        std::uint32_t decisions = 2;
        if (token.kind == LzssTypedTokenKind::match) {
            const auto length_class =
                lzss_field_context_value_class(token.length - 2);
            const auto distance_class =
                lzss_field_context_value_class(token.distance);
            events = static_cast<std::size_t>(3 + (length_class != 0)
                                               + (distance_class != 0));
            decisions = static_cast<std::uint32_t>(
                3 + length_class + distance_class);
        }
        if (!core::checked_add(result.operation_count, events,
                               result.operation_count)
            || !core::checked_add(result.decision_count, decisions,
                                  result.decision_count)) {
            result.error = LzssFieldContextError::arithmetic_overflow;
            return result;
        }
    }
    result.operation_index = result.operation_count;
    std::size_t bytes{};
    if (!core::checked_multiply(result.operation_count,
                                sizeof(ModeledOperation), bytes)) {
        result.error = LzssFieldContextError::arithmetic_overflow;
    } else if (bytes > limits.max_internal_buffered_bytes) {
        result.error = LzssFieldContextError::limit_exceeded;
    }
    return result;
}

LzssFieldContextResult model_lzss_short_match_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<ModeledOperation> operations) noexcept {
    auto result = plan_lzss_short_match_operations(
        tokens, parameters, context, limits);
    if (result.error != LzssFieldContextError::none) return result;
    if (operations.size() < result.operation_count) {
        result.error = LzssFieldContextError::output_too_small;
        return result;
    }
    const auto output = operations.first(result.operation_count);
    bool overflow{};
    const bool overlap = overlaps(tokens, output, overflow);
    if (overflow) {
        result.error = LzssFieldContextError::arithmetic_overflow;
        return result;
    }
    if (overlap) {
        result.error = LzssFieldContextError::overlapping_buffers;
        return result;
    }

    LzssFieldContextState state{};
    std::size_t index{};
    for (const auto& token : tokens) {
        write_symbol(output, index, state.token_context(),
                     token.kind == LzssTypedTokenKind::match ? 1U : 0U);
        if (token.kind == LzssTypedTokenKind::literal) {
            write_symbol(output, index, state.literal_context(),
                         token.literal);
        } else {
            const auto length_value = token.length - 2;
            const auto length_class =
                lzss_field_context_value_class(length_value);
            const auto distance_class =
                lzss_field_context_value_class(token.distance);
            write_symbol(output, index, state.length_context(), length_class);
            if (length_class != 0) {
                write_bypass(output, index, length_class,
                             length_value - (UINT32_C(1) << length_class));
            }
            write_symbol(output, index,
                         LzssFieldContextState::distance_context(length_class),
                         distance_class);
            if (distance_class != 0) {
                write_bypass(output, index, distance_class,
                             token.distance - (UINT32_C(1) << distance_class));
            }
        }
        state.accept(token);
    }
    result.operation_index = index;
    return result;
}

} // namespace marc::context::internal

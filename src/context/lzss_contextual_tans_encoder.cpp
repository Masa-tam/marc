#include "context/lzss_contextual_tans_encoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualTansEncodeError;
using entropy::internal::ContextualTansModelBuilder;
using entropy::internal::ContextualTansReverseWriter;

[[nodiscard]] std::uint8_t value_class(
    const std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

[[nodiscard]] LzssContextualTansEncodeResult fail_entropy(
    LzssContextualTansEncodeResult result,
    const ContextualTansEncodeError error) noexcept {
    result.entropy_error = error;
    if (error == ContextualTansEncodeError::limit_exceeded) {
        result.error = LzssContextualTansEncodeError::limit_exceeded;
    } else if (error == ContextualTansEncodeError::arithmetic_overflow) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
    } else if (error == ContextualTansEncodeError::table_output_too_small) {
        result.error = LzssContextualTansEncodeError::table_output_too_small;
    } else {
        result.error = LzssContextualTansEncodeError::entropy_error;
    }
    return result;
}

[[nodiscard]] bool add_event(
    LzssContextualTansEncodeResult& result) noexcept {
    if (result.event_count == std::numeric_limits<std::size_t>::max()) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return false;
    }
    ++result.event_count;
    return true;
}

[[nodiscard]] bool add_symbol(
    ContextualTansModelBuilder& builder,
    const std::uint16_t context_id,
    const std::uint16_t alphabet,
    const std::uint32_t value,
    LzssContextualTansEncodeResult& result) noexcept {
    const auto error = builder.add_symbol(context_id, alphabet, value);
    if (error != ContextualTansEncodeError::none) {
        result = fail_entropy(result, error);
        return false;
    }
    return add_event(result);
}

[[nodiscard]] bool add_bypass(
    ContextualTansModelBuilder& builder,
    const std::uint8_t bit_count,
    const std::uint32_t value,
    LzssContextualTansEncodeResult& result) noexcept {
    if (bit_count == 0) return true;
    const auto error = builder.add_bypass(bit_count, value);
    if (error != ContextualTansEncodeError::none) {
        result = fail_entropy(result, error);
        return false;
    }
    return add_event(result);
}

[[nodiscard]] bool add_token(
    ContextualTansModelBuilder& builder,
    const LzssFieldContextState& state,
    const LzssTypedToken& token,
    LzssContextualTansEncodeResult& result) noexcept {
    const auto kind = token.kind == LzssTypedTokenKind::literal ? 0U : 1U;
    if (!add_symbol(builder, state.token_context(), 2, kind, result)) {
        return false;
    }
    if (token.kind == LzssTypedTokenKind::literal) {
        return add_symbol(
            builder, state.literal_context(), 256, token.literal, result);
    }
    const auto length_value = token.length - 4U;
    const auto length_class = value_class(length_value);
    const auto length_extra =
        length_value - (UINT32_C(1) << length_class);
    const auto distance_class = value_class(token.distance);
    const auto distance_extra =
        token.distance - (UINT32_C(1) << distance_class);
    return add_symbol(
               builder, state.length_context(), 8, length_class, result)
        && add_bypass(builder, length_class, length_extra, result)
        && add_symbol(
            builder, LzssFieldContextState::distance_context(length_class),
            17, distance_class, result)
        && add_bypass(builder, distance_class, distance_extra, result);
}

[[nodiscard]] std::size_t find_literal_before(
    const std::span<const LzssTypedToken> tokens,
    std::size_t exclusive) noexcept {
    while (exclusive != 0) {
        --exclusive;
        if (tokens[exclusive].kind == LzssTypedTokenKind::literal) {
            return exclusive;
        }
    }
    return tokens.size();
}

struct ReverseContexts {
    std::uint16_t token{};
    std::uint16_t literal{3};

    [[nodiscard]] std::uint16_t length() const noexcept {
        return static_cast<std::uint16_t>(20 + token);
    }
};

[[nodiscard]] ReverseContexts contexts_before(
    const std::span<const LzssTypedToken> tokens,
    const std::size_t index,
    const std::size_t preceding_literal) noexcept {
    ReverseContexts contexts{};
    if (index != 0) {
        contexts.token = tokens[index - 1].kind
                == LzssTypedTokenKind::literal
            ? 1
            : 2;
    }
    if (preceding_literal != tokens.size()) {
        contexts.literal = static_cast<std::uint16_t>(
            4 + (tokens[preceding_literal].literal >> 4));
    }
    return contexts;
}

[[nodiscard]] ContextualTansEncodeError encode_token_reverse(
    ContextualTansReverseWriter& writer,
    const ReverseContexts contexts,
    const LzssTypedToken& token) noexcept {
    if (token.kind == LzssTypedTokenKind::literal) {
        auto error = writer.encode_symbol(
            contexts.literal, 256, token.literal);
        if (error != ContextualTansEncodeError::none) return error;
        return writer.encode_symbol(contexts.token, 2, 0);
    }
    const auto length_value = token.length - 4U;
    const auto length_class = value_class(length_value);
    const auto length_extra =
        length_value - (UINT32_C(1) << length_class);
    const auto distance_class = value_class(token.distance);
    const auto distance_extra =
        token.distance - (UINT32_C(1) << distance_class);
    auto error = distance_class == 0
        ? ContextualTansEncodeError::none
        : writer.encode_bypass(distance_class, distance_extra);
    if (error != ContextualTansEncodeError::none) return error;
    error = writer.encode_symbol(
        LzssFieldContextState::distance_context(length_class), 17,
        distance_class);
    if (error != ContextualTansEncodeError::none) return error;
    if (length_class != 0) {
        error = writer.encode_bypass(length_class, length_extra);
        if (error != ContextualTansEncodeError::none) return error;
    }
    error = writer.encode_symbol(contexts.length(), 8, length_class);
    if (error != ContextualTansEncodeError::none) return error;
    return writer.encode_symbol(contexts.token, 2, 1);
}

[[nodiscard]] ContextualTansEncodeError run_reverse(
    const std::span<const LzssTypedToken> tokens,
    const entropy::internal::ContextualTansDescriptor& descriptor,
    const std::span<const std::uint16_t> tables,
    const std::span<std::byte> output,
    std::size_t& payload_size,
    std::uint8_t& final_valid_bits) noexcept {
    ContextualTansReverseWriter writer(descriptor, tables, output);
    std::size_t preceding_literal = tokens.empty()
        ? tokens.size()
        : find_literal_before(tokens, tokens.size() - 1);
    for (std::size_t reverse = tokens.size(); reverse != 0; --reverse) {
        const auto index = reverse - 1;
        const auto error = encode_token_reverse(
            writer, contexts_before(tokens, index, preceding_literal),
            tokens[index]);
        if (error != ContextualTansEncodeError::none) return error;
        if (index != 0 && preceding_literal == index - 1) {
            preceding_literal = find_literal_before(tokens, index - 1);
        }
    }
    return writer.finish(payload_size, final_valid_bits);
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(
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

[[nodiscard]] LzssContextualTansEncodeResult fail_overlap(
    LzssContextualTansEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssContextualTansEncodeError::arithmetic_overflow
        : LzssContextualTansEncodeError::overlapping_buffers;
    return result;
}

} // namespace

LzssContextualTansEncodeResult plan_lzss_contextual_tans_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::uint16_t> private_encode_tables,
    entropy::internal::ContextualTansDescriptor& descriptor) noexcept {
    LzssContextualTansEncodeResult result{};
    std::size_t token_bytes{};
    std::size_t table_capacity_bytes{};
    std::size_t table_bytes{};
    if (!core::checked_multiply(
            tokens.size(), sizeof(LzssTypedToken), token_bytes)
        || !core::checked_multiply(
            private_encode_tables.size(), sizeof(std::uint16_t),
            table_capacity_bytes)
        || !core::checked_multiply(
            entropy::internal::contextual_tans_encode_table_entries,
            sizeof(std::uint16_t), table_bytes)) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return result;
    }
    const auto token_table = regions_overlap(
        tokens.data(), token_bytes, private_encode_tables.data(),
        table_capacity_bytes);
    if (token_table != OverlapCheck::disjoint) {
        return fail_overlap(result, token_table);
    }
    if (private_encode_tables.size()
        < entropy::internal::contextual_tans_encode_table_entries) {
        result.error = LzssContextualTansEncodeError::table_output_too_small;
        return result;
    }
    std::size_t minimum_buffered{};
    if (!core::checked_add(token_bytes, table_bytes, minimum_buffered)) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return result;
    }
    if (minimum_buffered > limits.max_internal_buffered_bytes) {
        result.error = LzssContextualTansEncodeError::limit_exceeded;
        return result;
    }

    result.token_validation = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits);
    result.token_count = result.token_validation.token_count;
    result.token_index = result.token_validation.token_index;
    if (result.token_validation.error
        != dictionary::internal::LzssTypedFrameValidationError::none) {
        result.error = result.token_validation.error
                == dictionary::internal::LzssTypedFrameValidationError::
                    arithmetic_overflow
            ? LzssContextualTansEncodeError::arithmetic_overflow
            : result.token_validation.error
                    == dictionary::internal::LzssTypedFrameValidationError::
                        limit_exceeded
                ? LzssContextualTansEncodeError::limit_exceeded
                : LzssContextualTansEncodeError::token_validation_error;
        return result;
    }

    ContextualTansModelBuilder builder;
    LzssFieldContextState state{};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        result.token_index = index;
        if (!add_token(builder, state, tokens[index], result)) return result;
        state.accept(tokens[index]);
    }
    result.token_index = result.token_count;
    result.decision_count = builder.decision_count();
    entropy::internal::ContextualTansDescriptor planned{};
    auto entropy_error = builder.finish(planned);
    if (entropy_error != ContextualTansEncodeError::none) {
        return fail_entropy(result, entropy_error);
    }
    entropy_error = entropy::internal::build_contextual_tans_encode_tables(
        planned, limits, private_encode_tables);
    if (entropy_error != ContextualTansEncodeError::none) {
        return fail_entropy(result, entropy_error);
    }
    std::uint8_t final_valid_bits{};
    entropy_error = run_reverse(
        tokens, planned,
        private_encode_tables.first(
            entropy::internal::contextual_tans_encode_table_entries),
        {}, result.payload_size, final_valid_bits);
    if (entropy_error != ContextualTansEncodeError::none) {
        return fail_entropy(result, entropy_error);
    }
    if (result.event_count > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return result;
    }
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    planned.final_valid_bits = final_valid_bits;
    std::size_t descriptor_size{};
    const auto format_error =
        entropy::internal::validate_contextual_tans_descriptor(
            planned, planned.decision_count, planned.payload_size, limits,
            descriptor_size);
    if (format_error
        == entropy::internal::ContextualTansFormatError::limit_exceeded) {
        result.error = LzssContextualTansEncodeError::limit_exceeded;
        return result;
    }
    if (format_error
        == entropy::internal::ContextualTansFormatError::arithmetic_overflow) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return result;
    }
    if (format_error != entropy::internal::ContextualTansFormatError::none) {
        result.error = LzssContextualTansEncodeError::internal_error;
        return result;
    }
    std::size_t buffered{};
    if (!core::checked_add(
            minimum_buffered, result.payload_size, buffered)) {
        result.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return result;
    }
    if (buffered > limits.max_internal_buffered_bytes) {
        result.error = LzssContextualTansEncodeError::limit_exceeded;
        return result;
    }
    descriptor = planned;
    return result;
}

LzssContextualTansEncodeResult encode_lzss_contextual_tans_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualTansDescriptor& descriptor) noexcept {
    LzssContextualTansEncodeResult initial{};
    std::size_t token_bytes{};
    std::size_t table_bytes{};
    if (!core::checked_multiply(
            tokens.size(), sizeof(LzssTypedToken), token_bytes)
        || !core::checked_multiply(
            private_encode_tables.size(), sizeof(std::uint16_t),
            table_bytes)) {
        initial.error = LzssContextualTansEncodeError::arithmetic_overflow;
        return initial;
    }
    for (const auto overlap : {
             regions_overlap(
                 tokens.data(), token_bytes, payload_output.data(),
                 payload_output.size()),
             regions_overlap(
                 private_encode_tables.data(), table_bytes,
                 payload_output.data(), payload_output.size())}) {
        if (overlap != OverlapCheck::disjoint) {
            return fail_overlap(initial, overlap);
        }
    }

    entropy::internal::ContextualTansDescriptor planned{};
    auto result = plan_lzss_contextual_tans_tokens(
        tokens, parameters, context, limits, private_encode_tables, planned);
    if (result.error != LzssContextualTansEncodeError::none) return result;
    if (payload_output.size() < result.payload_size) {
        result.error = LzssContextualTansEncodeError::payload_output_too_small;
        return result;
    }
    const auto output = payload_output.first(result.payload_size);
    std::fill(output.begin(), output.end(), std::byte{});
    std::size_t encoded_size{};
    std::uint8_t final_valid_bits{};
    const auto entropy_error = run_reverse(
        tokens, planned,
        private_encode_tables.first(
            entropy::internal::contextual_tans_encode_table_entries),
        output, encoded_size, final_valid_bits);
    if (entropy_error != ContextualTansEncodeError::none
        || encoded_size != result.payload_size
        || final_valid_bits != planned.final_valid_bits) {
        result.entropy_error = entropy_error;
        result.error = LzssContextualTansEncodeError::internal_error;
        return result;
    }
    descriptor = planned;
    return result;
}

} // namespace marc::context::internal

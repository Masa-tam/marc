#include "context/lzss_contextual_rans_encoder.hpp"

#include "entropy/contextual_rans_compact_format.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::context::internal {
namespace {

enum class DescriptorRepresentation : std::uint8_t {
    fixed,
    compact,
};

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualRansEncodeError;
using entropy::internal::ContextualRansModelBuilder;
using entropy::internal::ContextualRansReverseWriter;

[[nodiscard]] std::uint8_t value_class(
    const std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::bit_width(value) - 1U);
}

[[nodiscard]] LzssContextualRansEncodeResult fail_entropy(
    LzssContextualRansEncodeResult result,
    const ContextualRansEncodeError error) noexcept {
    result.entropy_error = error;
    if (error == ContextualRansEncodeError::limit_exceeded) {
        result.error = LzssContextualRansEncodeError::limit_exceeded;
    } else if (error == ContextualRansEncodeError::arithmetic_overflow) {
        result.error = LzssContextualRansEncodeError::arithmetic_overflow;
    } else {
        result.error = LzssContextualRansEncodeError::entropy_error;
    }
    return result;
}

[[nodiscard]] bool add_event(
    LzssContextualRansEncodeResult& result) noexcept {
    if (result.event_count == std::numeric_limits<std::size_t>::max()) {
        result.error = LzssContextualRansEncodeError::arithmetic_overflow;
        return false;
    }
    ++result.event_count;
    return true;
}

[[nodiscard]] bool add_symbol(
    ContextualRansModelBuilder& builder,
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value,
    LzssContextualRansEncodeResult& result) noexcept {
    const auto error = builder.add_symbol(context_id, alphabet, value);
    if (error != ContextualRansEncodeError::none) {
        result = fail_entropy(result, error);
        return false;
    }
    return add_event(result);
}

[[nodiscard]] bool add_bypass(
    ContextualRansModelBuilder& builder, const std::uint8_t bit_count,
    const std::uint32_t value,
    LzssContextualRansEncodeResult& result) noexcept {
    if (bit_count == 0) return true;
    const auto error = builder.add_bypass(bit_count, value);
    if (error != ContextualRansEncodeError::none) {
        result = fail_entropy(result, error);
        return false;
    }
    return add_event(result);
}

[[nodiscard]] bool add_token(
    ContextualRansModelBuilder& builder, const LzssFieldContextState& state,
    const LzssTypedToken& token,
    LzssContextualRansEncodeResult& result) noexcept {
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
    const std::span<const LzssTypedToken> tokens, const std::size_t index,
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

[[nodiscard]] ContextualRansEncodeError encode_token_reverse(
    ContextualRansReverseWriter& writer, const ReverseContexts contexts,
    const LzssTypedToken& token) noexcept {
    if (token.kind == LzssTypedTokenKind::literal) {
        auto error = writer.encode_symbol(
            contexts.literal, 256, token.literal);
        if (error != ContextualRansEncodeError::none) return error;
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
        ? ContextualRansEncodeError::none
        : writer.encode_bypass(distance_class, distance_extra);
    if (error != ContextualRansEncodeError::none) return error;
    error = writer.encode_symbol(
        LzssFieldContextState::distance_context(length_class), 17,
        distance_class);
    if (error != ContextualRansEncodeError::none) return error;
    if (length_class != 0) {
        error = writer.encode_bypass(length_class, length_extra);
        if (error != ContextualRansEncodeError::none) return error;
    }
    error = writer.encode_symbol(contexts.length(), 8, length_class);
    if (error != ContextualRansEncodeError::none) return error;
    return writer.encode_symbol(contexts.token, 2, 1);
}

[[nodiscard]] ContextualRansEncodeError run_reverse(
    const std::span<const LzssTypedToken> tokens,
    const entropy::internal::ContextualRansDescriptor& descriptor,
    const std::span<std::byte> output,
    std::size_t& payload_size) noexcept {
    ContextualRansReverseWriter writer(descriptor, output);
    std::size_t preceding_literal = tokens.empty()
        ? tokens.size()
        : find_literal_before(tokens, tokens.size() - 1);
    for (std::size_t reverse = tokens.size(); reverse != 0; --reverse) {
        const auto index = reverse - 1;
        const auto error = encode_token_reverse(
            writer, contexts_before(tokens, index, preceding_literal),
            tokens[index]);
        if (error != ContextualRansEncodeError::none) return error;
        if (index != 0 && preceding_literal == index - 1) {
            preceding_literal = find_literal_before(tokens, index - 1);
        }
    }
    return writer.finish(payload_size);
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck token_payload_overlap(
    const std::span<const LzssTypedToken> tokens,
    const std::span<std::byte> payload) noexcept {
    if (tokens.empty() || payload.empty()) return OverlapCheck::disjoint;
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            tokens.size(), sizeof(LzssTypedToken), token_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto token_begin = reinterpret_cast<std::uintptr_t>(tokens.data());
    const auto payload_begin = reinterpret_cast<std::uintptr_t>(payload.data());
    std::uintptr_t token_end{};
    std::uintptr_t payload_end{};
    if (!core::checked_add(
            token_begin, static_cast<std::uintptr_t>(token_bytes), token_end)
        || !core::checked_add(
            payload_begin, static_cast<std::uintptr_t>(payload.size()),
            payload_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return token_begin < payload_end && payload_begin < token_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

[[nodiscard]] LzssContextualRansEncodeResult plan_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualRansDescriptor& descriptor,
    const DescriptorRepresentation representation,
    std::size_t& descriptor_size) noexcept {
    LzssContextualRansEncodeResult result{};
    result.token_validation = dictionary::internal::validate_lzss_typed_frame(
        tokens, parameters, context, limits);
    result.token_count = result.token_validation.token_count;
    result.token_index = result.token_validation.token_index;
    if (result.token_validation.error
        != dictionary::internal::LzssTypedFrameValidationError::none) {
        result.error = result.token_validation.error
                == dictionary::internal::LzssTypedFrameValidationError::
                    arithmetic_overflow
            ? LzssContextualRansEncodeError::arithmetic_overflow
            : result.token_validation.error
                    == dictionary::internal::LzssTypedFrameValidationError::
                        limit_exceeded
                ? LzssContextualRansEncodeError::limit_exceeded
                : LzssContextualRansEncodeError::token_validation_error;
        return result;
    }

    ContextualRansModelBuilder builder;
    LzssFieldContextState state{};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        result.token_index = index;
        const auto& token = tokens[index];
        if (!add_token(builder, state, token, result)) return result;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.decision_count = builder.decision_count();
    entropy::internal::ContextualRansDescriptor planned{};
    auto entropy_error = builder.finish(planned);
    if (entropy_error != ContextualRansEncodeError::none) {
        return fail_entropy(result, entropy_error);
    }
    entropy_error = run_reverse(tokens, planned, {}, result.payload_size);
    if (entropy_error != ContextualRansEncodeError::none) {
        return fail_entropy(result, entropy_error);
    }
    if (result.event_count > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssContextualRansEncodeError::arithmetic_overflow;
        return result;
    }
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    if (representation == DescriptorRepresentation::compact) {
        const auto compact_error =
            entropy::internal::validate_contextual_rans_compact_descriptor(
                planned, planned.decision_count, planned.payload_size, limits,
                descriptor_size);
        if (compact_error
            == entropy::internal::ContextualRansCompactFormatError::
                limit_exceeded) {
            result.error = LzssContextualRansEncodeError::limit_exceeded;
            return result;
        }
        if (compact_error
            == entropy::internal::ContextualRansCompactFormatError::
                arithmetic_overflow) {
            result.error = LzssContextualRansEncodeError::arithmetic_overflow;
            return result;
        }
        if (compact_error
            != entropy::internal::ContextualRansCompactFormatError::none) {
            result.error = LzssContextualRansEncodeError::internal_error;
            return result;
        }
        descriptor = planned;
        return result;
    }

    const auto format_error =
        entropy::internal::validate_contextual_rans_descriptor(
            planned, planned.decision_count, planned.payload_size, limits);
    if (format_error
        == entropy::internal::ContextualRansFormatError::limit_exceeded) {
        result.error = LzssContextualRansEncodeError::limit_exceeded;
        return result;
    }
    if (format_error
        == entropy::internal::ContextualRansFormatError::arithmetic_overflow) {
        result.error = LzssContextualRansEncodeError::arithmetic_overflow;
        return result;
    }
    if (format_error != entropy::internal::ContextualRansFormatError::none) {
        result.error = LzssContextualRansEncodeError::internal_error;
        return result;
    }
    descriptor = planned;
    return result;
}

[[nodiscard]] LzssContextualRansEncodeResult encode_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualRansDescriptor& descriptor,
    const DescriptorRepresentation representation,
    std::size_t& descriptor_size) noexcept {
    entropy::internal::ContextualRansDescriptor planned{};
    const auto plan = plan_tokens(tokens, parameters, context, limits, planned,
                                  representation, descriptor_size);
    if (plan.error != LzssContextualRansEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        auto result = plan;
        result.error = LzssContextualRansEncodeError::payload_output_too_small;
        return result;
    }
    const auto output = payload_output.first(plan.payload_size);
    const auto overlap = token_payload_overlap(tokens, output);
    if (overlap != OverlapCheck::disjoint) {
        auto result = plan;
        result.error = overlap == OverlapCheck::arithmetic_overflow
            ? LzssContextualRansEncodeError::arithmetic_overflow
            : LzssContextualRansEncodeError::overlapping_buffers;
        return result;
    }
    std::size_t encoded_size{};
    const auto entropy_error = run_reverse(
        tokens, planned, output, encoded_size);
    if (entropy_error != ContextualRansEncodeError::none
        || encoded_size != plan.payload_size) {
        auto result = plan;
        result.error = LzssContextualRansEncodeError::internal_error;
        result.entropy_error = entropy_error;
        return result;
    }
    descriptor = planned;
    return plan;
}

} // namespace

LzssContextualRansEncodeResult plan_lzss_contextual_rans_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualRansDescriptor& descriptor) noexcept {
    std::size_t descriptor_size{};
    return plan_tokens(tokens, parameters, context, limits, descriptor,
                       DescriptorRepresentation::fixed, descriptor_size);
}

LzssContextualRansEncodeResult encode_lzss_contextual_rans_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualRansDescriptor& descriptor) noexcept {
    std::size_t descriptor_size{};
    return encode_tokens(tokens, parameters, context, limits, payload_output,
                         descriptor, DescriptorRepresentation::fixed,
                         descriptor_size);
}

LzssContextualRansEncodeResult plan_lzss_contextual_rans_compact_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualRansDescriptor& descriptor,
    std::size_t& descriptor_size) noexcept {
    return plan_tokens(tokens, parameters, context, limits, descriptor,
                       DescriptorRepresentation::compact, descriptor_size);
}

LzssContextualRansEncodeResult encode_lzss_contextual_rans_compact_tokens(
    const std::span<const LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    entropy::internal::ContextualRansDescriptor& descriptor,
    std::size_t& descriptor_size) noexcept {
    return encode_tokens(tokens, parameters, context, limits, payload_output,
                         descriptor, DescriptorRepresentation::compact,
                         descriptor_size);
}

} // namespace marc::context::internal

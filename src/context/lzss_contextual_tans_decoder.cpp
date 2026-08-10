#include "context/lzss_contextual_tans_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualTansDecodeError;
using entropy::internal::ContextualTansDecoder;
using entropy::internal::ContextualTansDescriptor;
using entropy::internal::TansDecodeEntry;

[[nodiscard]] bool read_symbol(
    ContextualTansDecoder& decoder, const std::uint16_t context,
    const std::uint16_t alphabet, std::uint32_t& value,
    LzssContextualTansDecodeResult& result) noexcept {
    result.entropy = decoder.decode_symbol(context, alphabet, value);
    if (result.entropy.error == ContextualTansDecodeError::none) return true;
    result.error = LzssContextualTansDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_bypass(
    ContextualTansDecoder& decoder, const std::uint8_t bits,
    std::uint32_t& value,
    LzssContextualTansDecodeResult& result) noexcept {
    result.entropy = decoder.decode_bypass(bits, value);
    if (result.entropy.error == ContextualTansDecodeError::none) return true;
    result.error = LzssContextualTansDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_token(
    ContextualTansDecoder& decoder, const LzssFieldContextState& state,
    LzssTypedToken& token,
    LzssContextualTansDecodeResult& result) noexcept {
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
            decoder, LzssFieldContextState::distance_context(length_class), 17,
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
    LzssContextualTansDecodeResult& result) noexcept {
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
        || decision_count > 26 * token_count
        || decision_count > 6 * raw_size) {
        result.error = LzssContextualTansDecodeError::invalid_counts;
        return false;
    }
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            static_cast<std::size_t>(context.declared_token_count),
            sizeof(LzssTypedToken), token_bytes)) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
        return false;
    }
    if (token_bytes > limits.max_internal_buffered_bytes
        || raw_size > limits.max_frame_size
        || raw_size > limits.max_block_size) {
        result.error = LzssContextualTansDecodeError::limit_exceeded;
        return false;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(
            context.output_already_committed, raw_size, total_output)) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
        return false;
    }
    if (total_output > limits.max_total_output_size) {
        result.error = LzssContextualTansDecodeError::limit_exceeded;
        return false;
    }
    return true;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck ranges_overlap(
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

[[nodiscard]] bool checked_table_bytes(
    const std::span<TansDecodeEntry> tables,
    std::size_t& bytes) noexcept {
    return core::checked_multiply(
        tables.size(), sizeof(TansDecodeEntry), bytes);
}

[[nodiscard]] bool checked_token_bytes(
    const std::span<LzssTypedToken> tokens,
    std::size_t& bytes) noexcept {
    return core::checked_multiply(
        tokens.size(), sizeof(LzssTypedToken), bytes);
}

[[nodiscard]] LzssContextualTansDecodeResult preflight_workspace(
    const std::span<const std::byte> payload,
    const std::span<TansDecodeEntry> tables) noexcept {
    LzssContextualTansDecodeResult result{};
    if (tables.size()
        < entropy::internal::contextual_tans_decode_table_entries) {
        result.error =
            LzssContextualTansDecodeError::table_output_too_small;
        return result;
    }
    const auto used_tables = tables.first(
        static_cast<std::size_t>(
            entropy::internal::contextual_tans_decode_table_entries));
    std::size_t table_bytes{};
    if (!checked_table_bytes(used_tables, table_bytes)) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
        return result;
    }
    const auto overlap = ranges_overlap(
        payload.data(), payload.size(), used_tables.data(), table_bytes);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
    } else if (overlap == OverlapCheck::overlap) {
        result.error = LzssContextualTansDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualTansDecodeResult preflight_token_output(
    const std::span<const std::byte> payload,
    const std::span<TansDecodeEntry> tables,
    const std::span<LzssTypedToken> private_tokens,
    const std::uint32_t declared_token_count,
    std::span<LzssTypedToken>& tokens) noexcept {
    LzssContextualTansDecodeResult result{};
    if (private_tokens.size() < declared_token_count) return result;
    tokens = private_tokens.first(declared_token_count);
    std::size_t table_bytes{};
    std::size_t token_bytes{};
    if (!checked_table_bytes(tables, table_bytes)
        || !checked_token_bytes(tokens, token_bytes)) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
        return result;
    }
    const auto payload_token = ranges_overlap(
        payload.data(), payload.size(), tokens.data(), token_bytes);
    const auto table_token = ranges_overlap(
        tables.data(), table_bytes, tokens.data(), token_bytes);
    if (payload_token == OverlapCheck::arithmetic_overflow
        || table_token == OverlapCheck::arithmetic_overflow) {
        result.error = LzssContextualTansDecodeError::arithmetic_overflow;
    } else if (payload_token == OverlapCheck::overlap
               || table_token == OverlapCheck::overlap) {
        result.error = LzssContextualTansDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualTansDecodeResult run_pass(
    const ContextualTansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<TansDecodeEntry> tables,
    const std::span<LzssTypedToken> output) noexcept {
    LzssContextualTansDecodeResult result{};
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssContextualTansDecodeError::limit_exceeded
            : LzssContextualTansDecodeError::invalid_parameters;
        return result;
    }
    if (!validate_declared_bounds(context, limits, result)) return result;
    if (descriptor.decision_count != context.declared_decision_count) {
        result.error = LzssContextualTansDecodeError::invalid_counts;
        return result;
    }

    ContextualTansDecoder decoder;
    result.entropy = decoder.begin(descriptor, payload, limits, tables);
    if (result.entropy.error != ContextualTansDecodeError::none) {
        result.error = result.entropy.error
                == ContextualTansDecodeError::table_output_too_small
            ? LzssContextualTansDecodeError::table_output_too_small
            : LzssContextualTansDecodeError::entropy_error;
        return result;
    }

    LzssFieldContextState state{};
    while (result.token_count < context.declared_token_count) {
        result.token_index = result.token_count;
        LzssTypedToken token{};
        if (!read_token(decoder, state, token, result)) return result;

        std::uint64_t next_raw_size{};
        result.token_error = dictionary::internal::validate_lzss_typed_token(
            token, parameters,
            {result.raw_size, context.declared_raw_size}, limits,
            next_raw_size);
        if (result.token_error != LzssTypedTokenError::none) {
            if (result.token_error == LzssTypedTokenError::limit_exceeded) {
                result.error = LzssContextualTansDecodeError::limit_exceeded;
            } else if (result.token_error
                       == LzssTypedTokenError::arithmetic_overflow) {
                result.error =
                    LzssContextualTansDecodeError::arithmetic_overflow;
            } else {
                result.error = LzssContextualTansDecodeError::invalid_token;
            }
            return result;
        }
        if (!output.empty()) output[result.token_count] = token;
        result.raw_size = next_raw_size;
        ++result.token_count;
        state.accept(token);
    }
    result.token_index = result.token_count;
    result.entropy = decoder.finish(
        context.declared_event_count, context.declared_decision_count);
    if (result.entropy.error != ContextualTansDecodeError::none) {
        result.error = LzssContextualTansDecodeError::entropy_error;
        return result;
    }
    if (result.raw_size != context.declared_raw_size) {
        result.error = LzssContextualTansDecodeError::raw_size_mismatch;
    }
    return result;
}

} // namespace

LzssContextualTansDecodeResult validate_lzss_contextual_tans_tokens(
    const entropy::internal::ContextualTansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::TansDecodeEntry> private_tables)
    noexcept {
    const auto workspace = preflight_workspace(payload, private_tables);
    if (workspace.error != LzssContextualTansDecodeError::none) {
        return workspace;
    }
    return run_pass(
        descriptor, payload, parameters, context, limits,
        private_tables.first(static_cast<std::size_t>(
            entropy::internal::contextual_tans_decode_table_entries)), {});
}

LzssContextualTansDecodeResult decode_lzss_contextual_tans_tokens(
    const entropy::internal::ContextualTansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::TansDecodeEntry> private_tables,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    const auto workspace = preflight_workspace(payload, private_tables);
    if (workspace.error != LzssContextualTansDecodeError::none) {
        return workspace;
    }
    const auto tables = private_tables.first(static_cast<std::size_t>(
        entropy::internal::contextual_tans_decode_table_entries));
    std::span<LzssTypedToken> tokens{};
    const auto token_workspace = preflight_token_output(
        payload, tables, private_tokens, context.declared_token_count, tokens);
    if (token_workspace.error != LzssContextualTansDecodeError::none) {
        return token_workspace;
    }

    auto result = run_pass(
        descriptor, payload, parameters, context, limits, tables, {});
    if (result.error != LzssContextualTansDecodeError::none) return result;
    if (private_tokens.size() < context.declared_token_count) {
        result.error =
            LzssContextualTansDecodeError::token_output_too_small;
        return result;
    }
    const auto decoded = run_pass(
        descriptor, payload, parameters, context, limits, tables, tokens);
    if (decoded.error != LzssContextualTansDecodeError::none
        || decoded.token_count != result.token_count
        || decoded.raw_size != result.raw_size
        || decoded.entropy.event_count != result.entropy.event_count
        || decoded.entropy.decision_count != result.entropy.decision_count
        || decoded.entropy.bits_consumed != result.entropy.bits_consumed) {
        result.error = LzssContextualTansDecodeError::internal_error;
        return result;
    }
    return decoded;
}

} // namespace marc::context::internal

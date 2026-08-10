#include "context/lzss_contextual_blocked_huffman_decoder.hpp"

#include "context/lzss_field_context_state.hpp"
#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::context::internal {
namespace {

using dictionary::internal::LzssTypedToken;
using dictionary::internal::LzssTypedTokenError;
using dictionary::internal::LzssTypedTokenKind;
using entropy::internal::ContextualBlockedHuffmanDecodeError;
using entropy::internal::ContextualBlockedHuffmanDecoder;
using entropy::internal::ContextualBlockedHuffmanDescriptor;
using entropy::internal::HuffmanDecodeTable;

[[nodiscard]] bool read_symbol(
    ContextualBlockedHuffmanDecoder& decoder, const std::uint16_t context,
    const std::uint16_t alphabet, std::uint32_t& value,
    LzssContextualBlockedHuffmanDecodeResult& result) noexcept {
    result.entropy = decoder.decode_symbol(context, alphabet, value);
    if (result.entropy.error
        == ContextualBlockedHuffmanDecodeError::none) {
        return true;
    }
    result.error = LzssContextualBlockedHuffmanDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_bypass(
    ContextualBlockedHuffmanDecoder& decoder, const std::uint8_t bits,
    std::uint32_t& value,
    LzssContextualBlockedHuffmanDecodeResult& result) noexcept {
    result.entropy = decoder.decode_bypass(bits, value);
    if (result.entropy.error
        == ContextualBlockedHuffmanDecodeError::none) {
        return true;
    }
    result.error = LzssContextualBlockedHuffmanDecodeError::entropy_error;
    return false;
}

[[nodiscard]] bool read_token(
    ContextualBlockedHuffmanDecoder& decoder,
    const LzssFieldContextState& state, LzssTypedToken& token,
    LzssContextualBlockedHuffmanDecodeResult& result) noexcept {
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
        && !read_bypass(
            decoder, static_cast<std::uint8_t>(length_class), length_extra,
            result)) {
        return false;
    }
    const auto length =
        (UINT32_C(1) << length_class) + length_extra + 4;

    std::uint32_t distance_class{};
    if (!read_symbol(
            decoder, LzssFieldContextState::distance_context(length_class),
            17, distance_class, result)) {
        return false;
    }
    std::uint32_t distance_extra{};
    if (distance_class != 0
        && !read_bypass(
            decoder, static_cast<std::uint8_t>(distance_class),
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
    LzssContextualBlockedHuffmanDecodeResult& result) noexcept {
    const auto token_count =
        static_cast<std::uint64_t>(context.declared_token_count);
    const auto event_count =
        static_cast<std::uint64_t>(context.declared_event_count);
    const auto decision_count =
        static_cast<std::uint64_t>(context.declared_decision_count);
    const auto raw_size = static_cast<std::uint64_t>(context.declared_raw_size);
    if (token_count == 0 || raw_size == 0 || token_count > raw_size
        || event_count < 2 * token_count || event_count > 5 * token_count
        || event_count > 2 * raw_size || decision_count < event_count
        || decision_count > 26 * token_count
        || decision_count > 6 * raw_size) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::invalid_counts;
        return false;
    }
    std::size_t token_bytes{};
    if (!core::checked_multiply(
            static_cast<std::size_t>(context.declared_token_count),
            sizeof(LzssTypedToken), token_bytes)) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
        return false;
    }
    if (token_bytes > limits.max_internal_buffered_bytes
        || raw_size > limits.max_frame_size
        || raw_size > limits.max_block_size) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::limit_exceeded;
        return false;
    }
    std::uint64_t total_output{};
    if (!core::checked_add(
            context.output_already_committed, raw_size, total_output)) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
        return false;
    }
    if (total_output > limits.max_total_output_size) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::limit_exceeded;
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
    const std::span<HuffmanDecodeTable> tables,
    std::size_t& bytes) noexcept {
    return core::checked_multiply(
        tables.size(), sizeof(HuffmanDecodeTable), bytes);
}

[[nodiscard]] bool checked_token_bytes(
    const std::span<LzssTypedToken> tokens, std::size_t& bytes) noexcept {
    return core::checked_multiply(
        tokens.size(), sizeof(LzssTypedToken), bytes);
}

[[nodiscard]] LzssContextualBlockedHuffmanDecodeResult preflight_workspace(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const std::span<HuffmanDecodeTable> private_tables,
    std::span<HuffmanDecodeTable>& tables) noexcept {
    LzssContextualBlockedHuffmanDecodeResult result{};
    const auto required = entropy::internal::
        contextual_blocked_huffman_required_decode_table_count(descriptor);
    if (private_tables.size() < required) {
        result.error = LzssContextualBlockedHuffmanDecodeError::
            table_output_too_small;
        return result;
    }
    tables = private_tables.first(required);
    std::size_t table_bytes{};
    if (!checked_table_bytes(tables, table_bytes)) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
        return result;
    }
    const auto overlap = ranges_overlap(
        payload.data(), payload.size(), tables.data(), table_bytes);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
    } else if (overlap == OverlapCheck::overlap) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualBlockedHuffmanDecodeResult
preflight_token_output(
    const std::span<const std::byte> payload,
    const std::span<HuffmanDecodeTable> tables,
    const std::span<LzssTypedToken> private_tokens,
    const std::uint32_t declared_token_count,
    std::span<LzssTypedToken>& tokens) noexcept {
    LzssContextualBlockedHuffmanDecodeResult result{};
    if (private_tokens.size() < declared_token_count) return result;
    tokens = private_tokens.first(declared_token_count);
    std::size_t table_bytes{};
    std::size_t token_bytes{};
    if (!checked_table_bytes(tables, table_bytes)
        || !checked_token_bytes(tokens, token_bytes)) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
        return result;
    }
    const auto payload_token = ranges_overlap(
        payload.data(), payload.size(), tokens.data(), token_bytes);
    const auto table_token = ranges_overlap(
        tables.data(), table_bytes, tokens.data(), token_bytes);
    if (payload_token == OverlapCheck::arithmetic_overflow
        || table_token == OverlapCheck::arithmetic_overflow) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::arithmetic_overflow;
    } else if (payload_token == OverlapCheck::overlap
               || table_token == OverlapCheck::overlap) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::overlapping_buffers;
    }
    return result;
}

[[nodiscard]] LzssContextualBlockedHuffmanDecodeResult run_pass(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<HuffmanDecodeTable> tables,
    const std::span<LzssTypedToken> output) noexcept {
    LzssContextualBlockedHuffmanDecodeResult result{};
    result.token_error = dictionary::internal::validate_lzss_typed_parameters(
        parameters, limits);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssContextualBlockedHuffmanDecodeError::limit_exceeded
            : LzssContextualBlockedHuffmanDecodeError::invalid_parameters;
        return result;
    }
    if (!validate_declared_bounds(context, limits, result)) return result;
    if (descriptor.decision_count != context.declared_decision_count) {
        result.error = LzssContextualBlockedHuffmanDecodeError::invalid_counts;
        return result;
    }

    ContextualBlockedHuffmanDecoder decoder;
    result.entropy = decoder.begin(descriptor, payload, limits, tables);
    if (result.entropy.error
        != ContextualBlockedHuffmanDecodeError::none) {
        result.error = result.entropy.error
                == ContextualBlockedHuffmanDecodeError::table_output_too_small
            ? LzssContextualBlockedHuffmanDecodeError::table_output_too_small
            : LzssContextualBlockedHuffmanDecodeError::entropy_error;
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
                result.error =
                    LzssContextualBlockedHuffmanDecodeError::limit_exceeded;
            } else if (result.token_error
                       == LzssTypedTokenError::arithmetic_overflow) {
                result.error = LzssContextualBlockedHuffmanDecodeError::
                    arithmetic_overflow;
            } else {
                result.error =
                    LzssContextualBlockedHuffmanDecodeError::invalid_token;
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
    if (result.entropy.error
        != ContextualBlockedHuffmanDecodeError::none) {
        result.error = LzssContextualBlockedHuffmanDecodeError::entropy_error;
        return result;
    }
    if (result.raw_size != context.declared_raw_size) {
        result.error =
            LzssContextualBlockedHuffmanDecodeError::raw_size_mismatch;
    }
    return result;
}

} // namespace

LzssContextualBlockedHuffmanDecodeResult
validate_lzss_contextual_blocked_huffman_tokens(
    const entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::HuffmanDecodeTable> private_tables)
    noexcept {
    std::span<HuffmanDecodeTable> tables{};
    const auto workspace = preflight_workspace(
        descriptor, payload, private_tables, tables);
    if (workspace.error
        != LzssContextualBlockedHuffmanDecodeError::none) {
        return workspace;
    }
    return run_pass(
        descriptor, payload, parameters, context, limits, tables, {});
}

LzssContextualBlockedHuffmanDecodeResult
decode_lzss_contextual_blocked_huffman_tokens(
    const entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    std::span<HuffmanDecodeTable> tables{};
    const auto workspace = preflight_workspace(
        descriptor, payload, private_tables, tables);
    if (workspace.error
        != LzssContextualBlockedHuffmanDecodeError::none) {
        return workspace;
    }
    std::span<LzssTypedToken> tokens{};
    const auto token_workspace = preflight_token_output(
        payload, tables, private_tokens, context.declared_token_count, tokens);
    if (token_workspace.error
        != LzssContextualBlockedHuffmanDecodeError::none) {
        return token_workspace;
    }

    auto result = run_pass(
        descriptor, payload, parameters, context, limits, tables, {});
    if (result.error != LzssContextualBlockedHuffmanDecodeError::none) {
        return result;
    }
    if (private_tokens.size() < context.declared_token_count) {
        result.error = LzssContextualBlockedHuffmanDecodeError::
            token_output_too_small;
        return result;
    }
    const auto decoded = run_pass(
        descriptor, payload, parameters, context, limits, tables, tokens);
    if (decoded.error != LzssContextualBlockedHuffmanDecodeError::none
        || decoded.token_count != result.token_count
        || decoded.raw_size != result.raw_size
        || decoded.entropy.event_count != result.entropy.event_count
        || decoded.entropy.decision_count != result.entropy.decision_count
        || decoded.entropy.bits_consumed != result.entropy.bits_consumed) {
        result.error = LzssContextualBlockedHuffmanDecodeError::internal_error;
        return result;
    }
    return decoded;
}

} // namespace marc::context::internal

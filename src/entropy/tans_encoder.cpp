#include "entropy/tans_encoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/rans_normalizer.hpp"
#include "entropy/tans_tables.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

struct PassResult {
    std::size_t bit_count{};
    std::uint16_t final_state{};
    TansEncodeError error{TansEncodeError::none};
};

[[nodiscard]] PassResult encode_pass(
    const std::span<const std::byte> input, const TansDescriptor& descriptor,
    const TansTables& tables, const std::span<std::byte> bit_output,
    const std::size_t expected_bits) noexcept {
    std::uint32_t state = tans_table_size;
    std::size_t bits{};
    std::size_t cursor = expected_bits;
    for (std::size_t reverse = input.size(); reverse != 0; --reverse) {
        const auto symbol = std::to_integer<std::uint8_t>(input[reverse - 1]);
        const auto frequency = descriptor.frequencies[symbol];
        if (frequency == 0) return {0, 0, TansEncodeError::internal_error};
        bool found{};
        std::uint8_t selected_bits{};
        std::uint32_t q{};
        for (std::uint8_t candidate = 0; candidate <= tans_table_log;
             ++candidate) {
            const auto reduced = state >> candidate;
            if (reduced < frequency || reduced >= 2U * frequency) continue;
            const auto index = static_cast<std::uint32_t>(
                tables.symbol_offsets[symbol]) + reduced - frequency;
            if (index >= tables.encode_states.size())
                return {0, 0, TansEncodeError::internal_error};
            const auto next = tables.encode_states[index];
            if (next < tans_table_size || next >= 2U * tans_table_size)
                return {0, 0, TansEncodeError::internal_error};
            if (tables.decode[next - tans_table_size].bit_count != candidate)
                continue;
            if (found) return {0, 0, TansEncodeError::internal_error};
            found = true;
            selected_bits = candidate;
            q = reduced;
        }
        if (!found) return {0, 0, TansEncodeError::internal_error};
        if (!core::checked_add(bits, static_cast<std::size_t>(selected_bits),
                               bits))
            return {0, 0, TansEncodeError::arithmetic_overflow};
        if (!bit_output.empty()) {
            if (cursor < selected_bits)
                return {0, 0, TansEncodeError::internal_error};
            cursor -= selected_bits;
            const auto value = state & ((UINT32_C(1) << selected_bits) - 1);
            for (std::uint8_t bit = 0; bit < selected_bits; ++bit) {
                const auto position = cursor + bit;
                if (((value >> bit) & 1U) != 0)
                    bit_output[position / 8] |=
                        static_cast<std::byte>(UINT8_C(1) << (position % 8));
            }
        }
        const auto index = static_cast<std::uint32_t>(
            tables.symbol_offsets[symbol]) + q - frequency;
        state = tables.encode_states[index];
    }
    if (!bit_output.empty() && (bits != expected_bits || cursor != 0))
        return {bits, 0, TansEncodeError::internal_error};
    return {bits, static_cast<std::uint16_t>(state), TansEncodeError::none};
}

} // namespace

TansEncodeResult plan_tans_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits, TansDescriptor& descriptor) noexcept {
    if (input.empty()) return {0, TansEncodeError::empty_input};
    if (input.size() > tans_max_block_size
        || input.size() > std::numeric_limits<std::uint32_t>::max())
        return {0, TansEncodeError::block_too_large};
    TansDescriptor planned{};
    const auto normalized = normalize_rans_frequencies(
        input, limits, planned.frequencies);
    if (normalized != RansNormalizationError::none)
        return {0, normalized == RansNormalizationError::limit_exceeded
                       ? TansEncodeError::limit_exceeded
                       : TansEncodeError::normalization_error};
    TansTables tables{};
    if (build_tans_tables(planned, tables) != TansTableError::none)
        return {0, TansEncodeError::table_error};
    const auto pass = encode_pass(input, planned, tables, {}, 0);
    if (pass.error != TansEncodeError::none) return {0, pass.error};
    std::size_t bit_bytes{};
    if (!core::checked_add(pass.bit_count, std::size_t{7}, bit_bytes))
        return {0, TansEncodeError::arithmetic_overflow};
    bit_bytes /= 8;
    std::size_t payload_size{};
    if (!core::checked_add(
            static_cast<std::size_t>(tans_min_payload_size), bit_bytes,
            payload_size)
        || payload_size > std::numeric_limits<std::uint32_t>::max())
        return {0, TansEncodeError::arithmetic_overflow};
    planned.symbol_count = static_cast<std::uint32_t>(input.size());
    planned.payload_size = static_cast<std::uint32_t>(payload_size);
    planned.final_valid_bits = pass.bit_count == 0 ? 0
        : static_cast<std::uint8_t>((pass.bit_count - 1) % 8 + 1);
    const auto validation = validate_tans_descriptor(
        planned, planned.symbol_count, planned.payload_size, limits);
    if (validation != TansFormatError::none)
        return {payload_size, validation == TansFormatError::limit_exceeded
                                 ? TansEncodeError::limit_exceeded
                                 : TansEncodeError::internal_error};
    descriptor = planned;
    return {payload_size, TansEncodeError::none};
}

TansEncodeResult encode_tans_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    TansDescriptor& descriptor) noexcept {
    TansDescriptor planned{};
    const auto plan = plan_tans_block(input, limits, planned);
    if (plan.error != TansEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size)
        return {plan.payload_size, TansEncodeError::payload_output_too_small};
    TansTables tables{};
    if (build_tans_tables(planned, tables) != TansTableError::none)
        return {plan.payload_size, TansEncodeError::internal_error};
    auto output = payload_output.first(plan.payload_size);
    std::fill(output.begin(), output.end(), std::byte{});
    const auto total_bits = output.size() == tans_min_payload_size ? 0
        : (output.size() - tans_min_payload_size - 1) * 8
            + planned.final_valid_bits;
    const auto pass = encode_pass(
        input, planned, tables, output.subspan(tans_min_payload_size),
        total_bits);
    if (pass.error != TansEncodeError::none
        || pass.final_state < tans_table_size
        || pass.final_state >= 2U * tans_table_size
        || !core::store_le(
            output, 0, static_cast<std::uint16_t>(
                pass.final_state - tans_table_size)))
        return {plan.payload_size, TansEncodeError::internal_error};
    descriptor = planned;
    return plan;
}

} // namespace marc::entropy::internal

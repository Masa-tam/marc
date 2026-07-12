#include "entropy/tans_decoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/tans_tables.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

struct PreparedBlock {
    TansTables tables{};
    std::size_t total_bits{};
    std::uint32_t initial_state{};
};

[[nodiscard]] TansDecodeResult prepare(
    const TansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits, PreparedBlock& prepared) noexcept {
    if (payload.size() > UINT32_MAX
        || descriptor.payload_size != payload.size())
        return {descriptor.symbol_count, 0,
                TansDecodeError::payload_size_mismatch};
    const auto format = validate_tans_descriptor(
        descriptor, descriptor.symbol_count,
        static_cast<std::uint32_t>(payload.size()), limits);
    if (format != TansFormatError::none)
        return {descriptor.symbol_count, 0,
                format == TansFormatError::contradictory_size
                    ? TansDecodeError::payload_size_mismatch
                    : TansDecodeError::invalid_descriptor};
    TansTables tables{};
    if (build_tans_tables(descriptor, tables) != TansTableError::none)
        return {descriptor.symbol_count, 0, TansDecodeError::invalid_table};
    std::uint16_t offset{};
    if (!core::load_le(payload, 0, offset) || offset >= tans_table_size)
        return {descriptor.symbol_count, 0, TansDecodeError::invalid_state};
    std::size_t total_bits{};
    if (payload.size() > tans_min_payload_size) {
        if (!core::checked_multiply(
                payload.size() - tans_min_payload_size - 1,
                std::size_t{8}, total_bits)
            || !core::checked_add(
                total_bits,
                static_cast<std::size_t>(descriptor.final_valid_bits),
                total_bits))
            return {descriptor.symbol_count, 0,
                    TansDecodeError::invalid_descriptor};
        const auto high_mask = static_cast<std::uint8_t>(
            0xffU << descriptor.final_valid_bits);
        if ((std::to_integer<std::uint8_t>(payload.back()) & high_mask) != 0)
            return {descriptor.symbol_count, 0,
                    TansDecodeError::nonzero_padding};
    }
    prepared.tables = tables;
    prepared.total_bits = total_bits;
    prepared.initial_state = tans_table_size + offset;
    return {descriptor.symbol_count, 0, TansDecodeError::none};
}

[[nodiscard]] TansDecodeResult run(
    const TansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const PreparedBlock& prepared,
    const std::span<std::byte> output) noexcept {
    std::uint32_t state = prepared.initial_state;
    std::size_t bit_offset{};
    for (std::size_t index = 0; index < descriptor.symbol_count; ++index) {
        if (state < tans_table_size || state >= 2U * tans_table_size)
            return {descriptor.symbol_count, bit_offset,
                    TansDecodeError::invalid_state};
        const auto entry = prepared.tables.decode[state - tans_table_size];
        if (entry.state_base < tans_table_size
            || entry.state_base >= 2U * tans_table_size)
            return {descriptor.symbol_count, bit_offset,
                    TansDecodeError::invalid_table};
        if (bit_offset > prepared.total_bits
            || entry.bit_count > prepared.total_bits - bit_offset)
            return {descriptor.symbol_count, bit_offset,
                    TansDecodeError::truncated_bits};
        std::uint32_t value{};
        for (std::uint8_t bit = 0; bit < entry.bit_count; ++bit) {
            const auto position = bit_offset + bit;
            const auto byte = std::to_integer<std::uint8_t>(
                payload[tans_min_payload_size + position / 8]);
            value |= static_cast<std::uint32_t>(
                (byte >> (position % 8)) & 1U) << bit;
        }
        bit_offset += entry.bit_count;
        state = entry.state_base + value;
        if (!output.empty()) output[index] = static_cast<std::byte>(entry.symbol);
    }
    if (state != tans_table_size)
        return {descriptor.symbol_count, bit_offset,
                TansDecodeError::invalid_terminal_state};
    if (bit_offset != prepared.total_bits)
        return {descriptor.symbol_count, bit_offset,
                TansDecodeError::trailing_bits};
    return {descriptor.symbol_count, bit_offset, TansDecodeError::none};
}

} // namespace

TansDecodeResult validate_tans_block(
    const TansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    PreparedBlock prepared{};
    const auto checked = prepare(descriptor, payload, limits, prepared);
    if (checked.error != TansDecodeError::none) return checked;
    return run(descriptor, payload, prepared, {});
}

TansDecodeResult decode_tans_block(
    const TansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    PreparedBlock prepared{};
    const auto checked = prepare(descriptor, payload, limits, prepared);
    if (checked.error != TansDecodeError::none) return checked;
    if (output.size() < descriptor.symbol_count)
        return {descriptor.symbol_count, 0, TansDecodeError::output_too_small};
    const auto validation = run(descriptor, payload, prepared, {});
    if (validation.error != TansDecodeError::none) return validation;
    const auto decoded = run(
        descriptor, payload, prepared, output.first(descriptor.symbol_count));
    if (decoded.error != TansDecodeError::none)
        return {descriptor.symbol_count, decoded.bits_consumed,
                TansDecodeError::internal_error};
    return decoded;
}

} // namespace marc::entropy::internal

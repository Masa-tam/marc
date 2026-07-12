#include "entropy/rans_decoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/rans_decode_table.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

struct DecodePassResult {
    std::size_t payload_consumed{};
    RansDecodeError error{RansDecodeError::none};
};

[[nodiscard]] bool boundary_state(const std::uint64_t state) noexcept {
    return state >= rans_lower_bound
        && state < rans_lower_bound * UINT64_C(256);
}

[[nodiscard]] DecodePassResult decode_pass(
    const RansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const RansDecodeTable& table,
    const std::span<std::byte> output) noexcept {
    std::uint64_t state{};
    if (!core::load_le(payload, 0, state)) {
        return {0, RansDecodeError::truncated_payload};
    }
    if (!boundary_state(state)) return {8, RansDecodeError::invalid_state};
    std::size_t offset = rans_min_payload_size;
    for (std::uint32_t produced = 0; produced < descriptor.symbol_count;
         ++produced) {
        if (!boundary_state(state)) {
            return {offset, RansDecodeError::invalid_state};
        }
        const auto slot = static_cast<std::uint32_t>(
            state & (rans_total_frequency - 1U));
        const auto entry = table[slot];
        if (entry.frequency == 0
            || slot < entry.cumulative
            || slot >= static_cast<std::uint32_t>(entry.cumulative)
                + entry.frequency) {
            return {offset, RansDecodeError::invalid_table};
        }
        std::uint64_t next{};
        if (!core::checked_multiply(
                static_cast<std::uint64_t>(entry.frequency),
                state >> rans_table_log, next)
            || !core::checked_add(
                next,
                static_cast<std::uint64_t>(slot - entry.cumulative),
                state)) {
            return {offset, RansDecodeError::arithmetic_overflow};
        }
        while (state < rans_lower_bound) {
            if (offset >= payload.size()) {
                return {offset, RansDecodeError::truncated_payload};
            }
            if (state > (UINT64_MAX >> 8)) {
                return {offset, RansDecodeError::arithmetic_overflow};
            }
            state = (state << 8)
                | std::to_integer<std::uint8_t>(payload[offset]);
            ++offset;
        }
        if (!boundary_state(state)) {
            return {offset, RansDecodeError::invalid_state};
        }
        if (!output.empty()) {
            output[produced] = static_cast<std::byte>(entry.symbol);
        }
    }
    if (state != rans_lower_bound) {
        return {offset, RansDecodeError::invalid_terminal_state};
    }
    if (offset != payload.size()) {
        return {offset, RansDecodeError::trailing_payload};
    }
    return {offset, RansDecodeError::none};
}

[[nodiscard]] RansDecodeResult preflight(
    const RansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    RansDecodeTable& table) noexcept {
    if (validate_rans_descriptor(
            descriptor, descriptor.symbol_count, descriptor.payload_size,
            limits) != RansFormatError::none) {
        return {descriptor.symbol_count, 0,
                RansDecodeError::invalid_descriptor};
    }
    core::FrameBounds bounds{};
    bounds.uncompressed_size = descriptor.symbol_count;
    bounds.dictionary_serialized_size = descriptor.symbol_count;
    bounds.compressed_payload_size = descriptor.payload_size;
    bounds.largest_block_size = descriptor.symbol_count;
    bounds.entropy_table_entries = rans_total_frequency;
    bounds.model_buffered_bytes = rans_descriptor_size;
    bounds.payload_buffered_bytes = descriptor.payload_size;
    bounds.block_count = 1;
    if (core::validate_frame_bounds(limits, bounds, 0)
        != core::LimitError::none) {
        return {descriptor.symbol_count, 0,
                RansDecodeError::invalid_descriptor};
    }
    if (payload.size() != descriptor.payload_size) {
        return {descriptor.symbol_count, 0,
                RansDecodeError::payload_size_mismatch};
    }
    if (build_rans_decode_table(descriptor, table)
        != RansDecodeTableError::none) {
        return {descriptor.symbol_count, 0, RansDecodeError::invalid_table};
    }
    return {descriptor.symbol_count, 0, RansDecodeError::none};
}

} // namespace

RansDecodeResult validate_rans_block(
    const RansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    RansDecodeTable table{};
    const auto checked = preflight(descriptor, payload, limits, table);
    if (checked.error != RansDecodeError::none) return checked;
    const auto validation = decode_pass(descriptor, payload, table, {});
    return {descriptor.symbol_count, validation.payload_consumed,
            validation.error};
}

RansDecodeResult decode_rans_block(
    const RansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    RansDecodeTable table{};
    const auto checked = preflight(descriptor, payload, limits, table);
    if (checked.error != RansDecodeError::none) return checked;
    if (output.size() < descriptor.symbol_count) {
        return {descriptor.symbol_count, 0, RansDecodeError::output_too_small};
    }
    const auto validation = decode_pass(descriptor, payload, table, {});
    if (validation.error != RansDecodeError::none) {
        return {descriptor.symbol_count, validation.payload_consumed,
                validation.error};
    }
    const auto decoded = decode_pass(
        descriptor, payload, table, output.first(descriptor.symbol_count));
    if (decoded.error != RansDecodeError::none
        || decoded.payload_consumed != validation.payload_consumed) {
        return {descriptor.symbol_count, decoded.payload_consumed,
                RansDecodeError::internal_error};
    }
    return {descriptor.symbol_count, decoded.payload_consumed,
            RansDecodeError::none};
}

} // namespace marc::entropy::internal

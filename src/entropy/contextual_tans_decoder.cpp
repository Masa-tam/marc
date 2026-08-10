#include "entropy/contextual_tans_decoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

void ContextualTansDecoder::reset() noexcept {
    payload_ = {};
    tables_ = {};
    active_contexts_.fill(false);
    requested_contexts_.fill(false);
    state_ = 0;
    total_bits_ = 0;
    bit_offset_ = 0;
    expected_decisions_ = 0;
    event_count_ = 0;
    decision_count_ = 0;
    error_ = ContextualTansDecodeError::none;
    started_ = false;
    finished_ = false;
}

ContextualTansDecodeResult ContextualTansDecoder::result() const noexcept {
    return {event_count_, decision_count_, bit_offset_, error_};
}

ContextualTansDecodeResult ContextualTansDecoder::fail(
    const ContextualTansDecodeError error) noexcept {
    error_ = error;
    return result();
}

ContextualTansDecodeResult ContextualTansDecoder::begin(
    const ContextualTansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<TansDecodeEntry> table_output) noexcept {
    reset();
    if (payload.size() != descriptor.payload_size) {
        return fail(ContextualTansDecodeError::payload_size_mismatch);
    }
    std::size_t descriptor_size{};
    if (validate_contextual_tans_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            limits, descriptor_size) != ContextualTansFormatError::none) {
        return fail(ContextualTansDecodeError::invalid_descriptor);
    }

    std::size_t total_bits{};
    if (payload.size() > tans_min_payload_size) {
        if (!core::checked_multiply(
                payload.size() - tans_min_payload_size - 1,
                std::size_t{8}, total_bits)
            || !core::checked_add(
                total_bits,
                static_cast<std::size_t>(descriptor.final_valid_bits),
                total_bits)) {
            return fail(ContextualTansDecodeError::invalid_descriptor);
        }
        const auto high_mask = static_cast<std::uint8_t>(
            0xffU << descriptor.final_valid_bits);
        if ((std::to_integer<std::uint8_t>(payload.back()) & high_mask) != 0) {
            return fail(ContextualTansDecodeError::nonzero_padding);
        }
    }

    std::uint16_t offset{};
    if (!core::load_le(payload, 0, offset)
        || offset >= contextual_tans_total_frequency) {
        return fail(ContextualTansDecodeError::invalid_state);
    }

    ContextualTansDecodeTables built{};
    const auto table_result = build_contextual_tans_decode_tables(
        descriptor, limits, table_output, built);
    if (table_result.error
        == ContextualTansDecodeTableError::output_too_small) {
        return fail(ContextualTansDecodeError::table_output_too_small);
    }
    if (table_result.error != ContextualTansDecodeTableError::none) {
        return fail(table_result.error
                            == ContextualTansDecodeTableError::invalid_descriptor
                        ? ContextualTansDecodeError::invalid_descriptor
                        : ContextualTansDecodeError::invalid_table);
    }

    payload_ = payload;
    tables_ = built.entries;
    active_contexts_ = built.active_contexts;
    state_ = contextual_tans_total_frequency + offset;
    total_bits_ = total_bits;
    expected_decisions_ = descriptor.decision_count;
    started_ = true;
    return result();
}

ContextualTansDecodeError ContextualTansDecoder::decode_transition(
    const std::size_t table_index,
    const std::uint16_t expected_alphabet,
    std::uint32_t& symbol) noexcept {
    if (state_ < contextual_tans_total_frequency
        || state_ >= 2U * contextual_tans_total_frequency
        || table_index > contextual_tans_bypass_table_index) {
        return ContextualTansDecodeError::invalid_state;
    }
    const auto& entry = tables_[
        table_index * contextual_tans_total_frequency
        + state_ - contextual_tans_total_frequency];
    if (entry.symbol >= expected_alphabet
        || entry.state_base < contextual_tans_total_frequency
        || entry.bit_count > contextual_tans_table_log) {
        return ContextualTansDecodeError::invalid_table;
    }
    const auto state_extent = UINT32_C(1) << entry.bit_count;
    if (entry.state_base >= 2U * contextual_tans_total_frequency
        || state_extent
               > 2U * contextual_tans_total_frequency - entry.state_base) {
        return ContextualTansDecodeError::invalid_table;
    }
    if (bit_offset_ > total_bits_
        || entry.bit_count > total_bits_ - bit_offset_) {
        return ContextualTansDecodeError::truncated_bits;
    }

    std::uint32_t bits{};
    for (std::uint8_t bit = 0; bit < entry.bit_count; ++bit) {
        const auto position = bit_offset_ + bit;
        const auto byte = std::to_integer<std::uint8_t>(
            payload_[tans_min_payload_size + position / 8]);
        bits |= static_cast<std::uint32_t>(
            (byte >> (position % 8)) & 1U) << bit;
    }
    state_ = entry.state_base + bits;
    bit_offset_ += entry.bit_count;
    symbol = entry.symbol;
    return ContextualTansDecodeError::none;
}

ContextualTansDecodeResult ContextualTansDecoder::decode_symbol(
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet,
    std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualTansDecodeError::none
            || error_ == ContextualTansDecodeError::not_started) {
            return fail(ContextualTansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualTansDecodeError::none) return result();
    if (finished_) return fail(ContextualTansDecodeError::already_finished);
    if (expected_context >= contextual_tans_context_count) {
        return fail(ContextualTansDecodeError::invalid_context);
    }
    if (expected_alphabet
        != context::internal::lzss_field_context_alphabets[expected_context]) {
        return fail(ContextualTansDecodeError::invalid_alphabet);
    }
    if (!active_contexts_[expected_context]) {
        return fail(ContextualTansDecodeError::inactive_context);
    }
    if (decision_count_ >= expected_decisions_) {
        return fail(ContextualTansDecodeError::decision_count_exceeded);
    }

    std::uint32_t decoded{};
    const auto error = decode_transition(
        expected_context, expected_alphabet, decoded);
    if (error != ContextualTansDecodeError::none) return fail(error);
    requested_contexts_[expected_context] = true;
    ++event_count_;
    ++decision_count_;
    value = decoded;
    return result();
}

ContextualTansDecodeResult ContextualTansDecoder::decode_bypass(
    const std::uint8_t expected_bit_count, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualTansDecodeError::none
            || error_ == ContextualTansDecodeError::not_started) {
            return fail(ContextualTansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualTansDecodeError::none) return result();
    if (finished_) return fail(ContextualTansDecodeError::already_finished);
    if (expected_bit_count == 0 || expected_bit_count > 16) {
        return fail(ContextualTansDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > expected_decisions_
        || expected_bit_count > expected_decisions_ - decision_count_) {
        return fail(ContextualTansDecodeError::decision_count_exceeded);
    }

    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < expected_bit_count;
         ++bit_index) {
        std::uint32_t bit{};
        const auto error = decode_transition(
            contextual_tans_bypass_table_index, 2, bit);
        if (error != ContextualTansDecodeError::none) return fail(error);
        decoded |= bit << bit_index;
        ++decision_count_;
    }
    ++event_count_;
    value = decoded;
    return result();
}

ContextualTansDecodeResult ContextualTansDecoder::finish(
    const std::uint32_t expected_event_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (!started_) {
        if (error_ == ContextualTansDecodeError::none
            || error_ == ContextualTansDecodeError::not_started) {
            return fail(ContextualTansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualTansDecodeError::none) return result();
    if (finished_) return fail(ContextualTansDecodeError::already_finished);
    if (event_count_ != expected_event_count
        || decision_count_ != expected_decision_count
        || decision_count_ != expected_decisions_) {
        return fail(ContextualTansDecodeError::count_mismatch);
    }
    for (std::size_t context_id = 0;
         context_id < active_contexts_.size(); ++context_id) {
        if (active_contexts_[context_id]
            && !requested_contexts_[context_id]) {
            return fail(ContextualTansDecodeError::unused_context);
        }
    }
    if (state_ != contextual_tans_total_frequency) {
        return fail(ContextualTansDecodeError::invalid_terminal_state);
    }
    if (bit_offset_ != total_bits_) {
        return fail(ContextualTansDecodeError::trailing_bits);
    }
    finished_ = true;
    return result();
}

} // namespace marc::entropy::internal

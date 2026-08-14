#include "entropy/contextual_rans_decoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

void ContextualRansDecoder::reset() noexcept {
    payload_ = {};
    tables_ = {};
    layout_ = {};
    active_contexts_.fill(false);
    requested_contexts_.fill(false);
    state_ = 0;
    payload_offset_ = 0;
    expected_decisions_ = 0;
    event_count_ = 0;
    decision_count_ = 0;
    error_ = ContextualRansDecodeError::none;
    started_ = false;
    finished_ = false;
}

ContextualRansDecodeResult ContextualRansDecoder::result() const noexcept {
    return {event_count_, decision_count_, payload_offset_, error_};
}

ContextualRansDecodeResult ContextualRansDecoder::fail(
    const ContextualRansDecodeError error) noexcept {
    error_ = error;
    return result();
}

bool ContextualRansDecoder::boundary_state() const noexcept {
    return state_ >= rans_lower_bound
        && state_ < rans_lower_bound * UINT64_C(256);
}

ContextualRansBeginResult ContextualRansDecoder::begin(
    const std::span<const std::byte> serialized_descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<RansDecodeEntry> table_output,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    reset();
    ContextualRansDescriptor descriptor{};
    const auto format_error = parse_contextual_rans_descriptor(
        serialized_descriptor, expected_decision_count, expected_payload_size,
        limits, descriptor, variant);
    if (format_error != ContextualRansFormatError::none) {
        return {fail(ContextualRansDecodeError::invalid_descriptor),
                format_error};
    }
    return {begin_validated(descriptor, payload, table_output, variant),
            ContextualRansFormatError::none};
}

ContextualRansDecodeResult ContextualRansDecoder::begin_validated(
    const ContextualRansDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const std::span<RansDecodeEntry> table_output,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    if (payload.size() != descriptor.payload_size) {
        return fail(ContextualRansDecodeError::payload_size_mismatch);
    }
    if (!core::load_le(payload, 0, state_)) {
        return fail(ContextualRansDecodeError::truncated_payload);
    }
    payload_offset_ = rans_min_payload_size;
    if (!boundary_state()) {
        return fail(ContextualRansDecodeError::invalid_state);
    }

    ContextualRansDecodeTables built{};
    const auto table_result = build_contextual_rans_decode_tables_from_model(
        descriptor, table_output, built, variant);
    if (table_result.error
        == ContextualRansDecodeTableError::output_too_small) {
        return fail(ContextualRansDecodeError::table_output_too_small);
    }
    if (table_result.error != ContextualRansDecodeTableError::none) {
        return fail(ContextualRansDecodeError::invalid_descriptor);
    }
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return fail(ContextualRansDecodeError::invalid_descriptor);
    }

    payload_ = payload;
    tables_ = built.entries;
    layout_ = selected.layout;
    active_contexts_ = built.active_contexts;
    expected_decisions_ = descriptor.decision_count;
    started_ = true;
    return result();
}

ContextualRansDecodeError ContextualRansDecoder::decode_range(
    const std::uint16_t cumulative,
    const std::uint16_t frequency) noexcept {
    if (!boundary_state() || frequency == 0
        || static_cast<std::uint32_t>(cumulative) + frequency
               > contextual_rans_total_frequency) {
        return ContextualRansDecodeError::invalid_state;
    }
    const auto slot = static_cast<std::uint32_t>(
        state_ & (contextual_rans_total_frequency - 1U));
    if (slot < cumulative
        || slot >= static_cast<std::uint32_t>(cumulative) + frequency) {
        return ContextualRansDecodeError::invalid_table;
    }
    std::uint64_t next{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(frequency),
            state_ >> contextual_rans_table_log, next)
        || !core::checked_add(
            next, static_cast<std::uint64_t>(slot - cumulative), state_)) {
        return ContextualRansDecodeError::arithmetic_overflow;
    }
    while (state_ < rans_lower_bound) {
        if (payload_offset_ >= payload_.size()) {
            return ContextualRansDecodeError::truncated_payload;
        }
        if (state_ > (UINT64_MAX >> 8)) {
            return ContextualRansDecodeError::arithmetic_overflow;
        }
        state_ = (state_ << 8)
            | std::to_integer<std::uint8_t>(payload_[payload_offset_++]);
    }
    if (!boundary_state()) return ContextualRansDecodeError::invalid_state;
    return ContextualRansDecodeError::none;
}

ContextualRansDecodeResult ContextualRansDecoder::decode_symbol(
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualRansDecodeError::none
            || error_ == ContextualRansDecodeError::not_started) {
            return fail(ContextualRansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualRansDecodeError::none) return result();
    if (finished_) return fail(ContextualRansDecodeError::already_finished);
    if (expected_context >= contextual_rans_context_count) {
        return fail(ContextualRansDecodeError::invalid_context);
    }
    if (expected_alphabet
        != (*layout_.alphabets)[expected_context]) {
        return fail(ContextualRansDecodeError::invalid_alphabet);
    }
    if (!active_contexts_[expected_context]) {
        return fail(ContextualRansDecodeError::inactive_context);
    }
    if (decision_count_ >= expected_decisions_) {
        return fail(ContextualRansDecodeError::decision_count_exceeded);
    }

    const auto slot = static_cast<std::uint32_t>(
        state_ & (contextual_rans_total_frequency - 1U));
    const auto& entry = tables_[
        static_cast<std::size_t>(expected_context)
            * contextual_rans_total_frequency
        + slot];
    if (entry.symbol >= expected_alphabet || entry.frequency == 0
        || slot < entry.cumulative
        || slot >= static_cast<std::uint32_t>(entry.cumulative)
                + entry.frequency) {
        return fail(ContextualRansDecodeError::invalid_table);
    }
    const auto decoded = entry.symbol;
    const auto error = decode_range(entry.cumulative, entry.frequency);
    if (error != ContextualRansDecodeError::none) return fail(error);

    requested_contexts_[expected_context] = true;
    ++event_count_;
    ++decision_count_;
    value = decoded;
    return result();
}

ContextualRansDecodeResult ContextualRansDecoder::decode_bypass(
    const std::uint8_t expected_bit_count, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualRansDecodeError::none
            || error_ == ContextualRansDecodeError::not_started) {
            return fail(ContextualRansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualRansDecodeError::none) return result();
    if (finished_) return fail(ContextualRansDecodeError::already_finished);
    if (expected_bit_count == 0
        || expected_bit_count > layout_.maximum_bypass_bits) {
        return fail(ContextualRansDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > expected_decisions_
        || expected_bit_count > expected_decisions_ - decision_count_) {
        return fail(ContextualRansDecodeError::decision_count_exceeded);
    }

    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < expected_bit_count;
         ++bit_index) {
        const auto slot = static_cast<std::uint32_t>(
            state_ & (contextual_rans_total_frequency - 1U));
        const auto bit = static_cast<std::uint32_t>(
            slot >= contextual_rans_total_frequency / 2U);
        const auto cumulative = static_cast<std::uint16_t>(
            bit * (contextual_rans_total_frequency / 2U));
        const auto error = decode_range(
            cumulative,
            static_cast<std::uint16_t>(contextual_rans_total_frequency / 2U));
        if (error != ContextualRansDecodeError::none) return fail(error);
        decoded |= bit << bit_index;
        ++decision_count_;
    }
    ++event_count_;
    value = decoded;
    return result();
}

ContextualRansDecodeResult ContextualRansDecoder::finish(
    const std::uint32_t expected_event_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (!started_) {
        if (error_ == ContextualRansDecodeError::none
            || error_ == ContextualRansDecodeError::not_started) {
            return fail(ContextualRansDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualRansDecodeError::none) return result();
    if (finished_) return fail(ContextualRansDecodeError::already_finished);
    if (event_count_ != expected_event_count
        || decision_count_ != expected_decision_count
        || decision_count_ != expected_decisions_) {
        return fail(ContextualRansDecodeError::count_mismatch);
    }
    for (std::size_t context = 0; context < active_contexts_.size(); ++context) {
        if (active_contexts_[context] && !requested_contexts_[context]) {
            return fail(ContextualRansDecodeError::unused_context);
        }
    }
    if (state_ != rans_lower_bound) {
        return fail(ContextualRansDecodeError::invalid_terminal_state);
    }
    if (payload_offset_ != payload_.size()) {
        return fail(ContextualRansDecodeError::trailing_payload);
    }
    finished_ = true;
    return result();
}

} // namespace marc::entropy::internal

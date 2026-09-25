#include "entropy/lzss_reduced_literal_range_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

} // namespace

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::result()
    const noexcept {
    return {state_.event_count, state_.decision_count, state_.payload_offset, state_.error};
}

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::fail(
    const ContextualDynamicRangeDecodeError error) noexcept {
    state_.error = error;
    return result();
}

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::begin(
    const ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    state_.payload = {};
    state_.descriptor = {};
    state_.payload_offset = 0;
    state_.code = 0;
    state_.range = UINT32_MAX;
    state_.event_count = 0;
    state_.decision_count = 0;
    state_.canonical_low = 0;
    state_.canonical_pending = 1;
    state_.canonical_offset = 0;
    state_.canonical_cache = 0;
    state_.canonical_mismatch = false;
    state_.error = ContextualDynamicRangeDecodeError::none;
    state_.started = false;
    state_.finished = false;
    state_.frequencies.fill(1);
    for (std::size_t index = 0; index < state_.totals.size(); ++index) {
        state_.totals[index] = context::internal::lzss_reduced_literal_alphabets[index];
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count == 0 || descriptor.payload_size < 5
        || descriptor.context_count
               != context::internal::lzss_reduced_literal_context_count
        || descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes
        || static_cast<std::uint64_t>(descriptor.payload_size) + sizeof(*this)
               > limits.max_internal_buffered_bytes
        || context::internal::lzss_reduced_literal_frequency_entries
               > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit
               > limits.max_range_model_total) {
        return fail(ContextualDynamicRangeDecodeError::invalid_descriptor);
    }
    if (payload.size() != descriptor.payload_size) {
        return fail(ContextualDynamicRangeDecodeError::payload_size_mismatch);
    }
    state_.payload = payload;
    state_.descriptor = descriptor;
    for (int index = 0; index < 5; ++index) {
        if (state_.payload_offset >= state_.payload.size()) {
            return fail(ContextualDynamicRangeDecodeError::truncated_payload);
        }
        const auto byte =
            std::to_integer<std::uint8_t>(state_.payload[state_.payload_offset++]);
        if (index == 0 && byte != 0) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        state_.code = static_cast<std::uint32_t>((state_.code << 8) | byte);
    }
    state_.started = true;
    return result();
}

bool LzssReducedLiteralRangeDecoder::decode_interval(
    const std::uint32_t cumulative, const std::uint16_t frequency,
    const std::uint32_t total) noexcept {
    if (total == 0 || frequency == 0 || cumulative >= total
        || static_cast<std::uint64_t>(cumulative) + frequency > total
        || state_.range < normalization_threshold) {
        return false;
    }
    const auto unit = state_.range / total;
    if (unit == 0) return false;
    state_.canonical_low += static_cast<std::uint64_t>(cumulative) * unit;
    state_.code -= cumulative * unit;
    state_.range = unit * frequency;
    while (state_.range < normalization_threshold) {
        if (state_.payload_offset >= state_.payload.size()) return false;
        const auto byte =
            std::to_integer<std::uint8_t>(state_.payload[state_.payload_offset++]);
        state_.range <<= 8;
        if (!canonical_shift_low()) return false;
        state_.code = static_cast<std::uint32_t>((state_.code << 8) | byte);
    }
    return true;
}

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::decode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    std::uint32_t& value) noexcept {
    if (!state_.started) {
        if (state_.error == ContextualDynamicRangeDecodeError::none
            || state_.error == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (state_.error != ContextualDynamicRangeDecodeError::none) return result();
    if (state_.finished) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (context_id >= context::internal::lzss_reduced_literal_context_count) {
        return fail(ContextualDynamicRangeDecodeError::invalid_context);
    }
    if (alphabet != context::internal::lzss_reduced_literal_alphabets[context_id]) {
        return fail(ContextualDynamicRangeDecodeError::invalid_alphabet);
    }
    if (state_.decision_count >= state_.descriptor.decision_count) {
        return fail(ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }
    const auto offset = context::internal::lzss_reduced_literal_offsets[context_id];
    const auto total = state_.totals[context_id];
    const auto unit = state_.range / total;
    if (state_.range < normalization_threshold || unit == 0) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    const auto scaled = state_.code / unit;
    if (scaled >= total) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    std::uint32_t cumulative{};
    std::uint32_t decoded{};
    for (; decoded < alphabet; ++decoded) {
        const auto frequency = state_.frequencies[offset + decoded];
        if (scaled < cumulative + frequency) break;
        cumulative += frequency;
    }
    if (decoded >= alphabet) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    auto& frequency = state_.frequencies[offset + decoded];
    if (!decode_interval(cumulative, frequency, total)) {
        return fail(state_.payload_offset >= state_.payload.size()
                        ? ContextualDynamicRangeDecodeError::truncated_payload
                        : ContextualDynamicRangeDecodeError::invalid_interval);
    }
    ++frequency;
    auto& updated_total = state_.totals[context_id];
    ++updated_total;
    if (updated_total == contextual_dynamic_range_model_total_limit) {
        updated_total = 0;
        for (std::size_t index = 0; index < alphabet; ++index) {
            auto& current = state_.frequencies[offset + index];
            current = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(current) + 1U) / 2U);
            updated_total += current;
        }
    }
    ++state_.event_count;
    ++state_.decision_count;
    value = decoded;
    return result();
}

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::decode_bypass(
    const std::uint8_t bit_count, std::uint32_t& value) noexcept {
    if (!state_.started) {
        if (state_.error == ContextualDynamicRangeDecodeError::none
            || state_.error == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (state_.error != ContextualDynamicRangeDecodeError::none) return result();
    if (state_.finished) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (bit_count == 0 || bit_count > 16) {
        return fail(ContextualDynamicRangeDecodeError::invalid_bypass_width);
    }
    if (state_.decision_count > state_.descriptor.decision_count
        || bit_count > state_.descriptor.decision_count - state_.decision_count) {
        return fail(ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }
    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < bit_count; ++bit_index) {
        const auto unit = state_.range / 2U;
        if (state_.range < normalization_threshold || unit == 0) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        const auto scaled = state_.code / unit;
        if (scaled >= 2) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        const auto bit = static_cast<std::uint32_t>(scaled);
        if (!decode_interval(bit, 1, 2)) {
            return fail(state_.payload_offset >= state_.payload.size()
                            ? ContextualDynamicRangeDecodeError::truncated_payload
                            : ContextualDynamicRangeDecodeError::invalid_interval);
        }
        decoded |= bit << bit_index;
        ++state_.decision_count;
    }
    ++state_.event_count;
    value = decoded;
    return result();
}

void LzssReducedLiteralRangeDecoder::canonical_emit(const std::uint8_t value) noexcept {
    if (state_.canonical_offset >= state_.payload.size()) {
        state_.canonical_mismatch = true;
        return;
    }
    if (state_.payload[state_.canonical_offset] != static_cast<std::byte>(value)) {
        state_.canonical_mismatch = true;
    }
    ++state_.canonical_offset;
}

bool LzssReducedLiteralRangeDecoder::canonical_shift_low() noexcept {
    const auto low = static_cast<std::uint32_t>(state_.canonical_low);
    const auto carry = static_cast<std::uint32_t>(state_.canonical_low >> 32);
    if (carry > 1) return false;
    if (low < UINT32_C(0xff000000) || carry != 0) {
        canonical_emit(static_cast<std::uint8_t>(state_.canonical_cache + carry));
        for (std::size_t i = 1; i < state_.canonical_pending; ++i) {
            canonical_emit(static_cast<std::uint8_t>(UINT32_C(0xff) + carry));
        }
        state_.canonical_cache = static_cast<std::uint8_t>(low >> 24);
        state_.canonical_pending = 0;
    }
    if (state_.canonical_pending == std::numeric_limits<std::size_t>::max()) return false;
    ++state_.canonical_pending;
    state_.canonical_low = static_cast<std::uint32_t>(low << 8);
    return true;
}

bool LzssReducedLiteralRangeDecoder::validate_models() const noexcept {
    for (std::size_t context_id = 0; context_id < state_.totals.size(); ++context_id) {
        const auto alphabet =
            context::internal::lzss_reduced_literal_alphabets[context_id];
        if (state_.totals[context_id] < alphabet
            || state_.totals[context_id] >= contextual_dynamic_range_model_total_limit) {
            return false;
        }
        std::uint32_t sum{};
        for (std::size_t symbol = 0; symbol < alphabet; ++symbol) {
            const auto frequency = state_.frequencies[
                context::internal::lzss_reduced_literal_offsets[context_id] + symbol];
            if (frequency == 0) return false;
            sum += frequency;
        }
        if (sum != state_.totals[context_id]) return false;
    }
    return true;
}

ContextualDynamicRangeDecodeResult LzssReducedLiteralRangeDecoder::finish(
    const std::uint32_t event_count,
    const std::uint32_t decision_count) noexcept {
    if (!state_.started) {
        if (state_.error == ContextualDynamicRangeDecodeError::none
            || state_.error == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (state_.error != ContextualDynamicRangeDecodeError::none) return result();
    if (state_.finished) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (state_.event_count != event_count || state_.decision_count != decision_count
        || state_.decision_count != state_.descriptor.decision_count) {
        return fail(ContextualDynamicRangeDecodeError::count_mismatch);
    }
    if (state_.payload_offset != state_.payload.size()) {
        return fail(ContextualDynamicRangeDecodeError::trailing_payload);
    }
    if (!validate_models()) {
        return fail(ContextualDynamicRangeDecodeError::invalid_model);
    }
    for (int i = 0; i < 5; ++i) {
        if (!canonical_shift_low()) return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    if (state_.canonical_mismatch || state_.canonical_offset != state_.payload.size()) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    state_.finished = true;
    return result();
}

} // namespace marc::entropy::internal

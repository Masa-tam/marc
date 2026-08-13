#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

} // namespace

ContextualDynamicRangeDecoder::ContextualDynamicRangeDecoder() noexcept {
    reset_models();
}

ContextualDynamicRangeDecodeResult ContextualDynamicRangeDecoder::result()
    const noexcept {
    return {event_count_, decision_count_, payload_offset_, error_};
}

ContextualDynamicRangeDecodeResult ContextualDynamicRangeDecoder::fail(
    const ContextualDynamicRangeDecodeError error) noexcept {
    error_ = error;
    return result();
}

void ContextualDynamicRangeDecoder::reset_models() noexcept {
    frequencies_.fill(1);
    if (layout_.alphabets == nullptr) {
        totals_.fill(0);
        return;
    }
    for (std::size_t index = 0; index < totals_.size(); ++index) {
        totals_[index] = (*layout_.alphabets)[index];
    }
}

ContextualDynamicRangeDecodeResult ContextualDynamicRangeDecoder::begin(
    const ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    payload_ = {};
    descriptor_ = {};
    payload_offset_ = 0;
    code_ = 0;
    range_ = UINT32_MAX;
    event_count_ = 0;
    decision_count_ = 0;
    error_ = ContextualDynamicRangeDecodeError::none;
    started_ = false;
    finished_ = false;
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    layout_ = selected.layout;
    reset_models();

    if (selected.error
            != context::internal::LzssFieldContextLayoutError::none
        || core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count == 0 || descriptor.payload_size < 5
        || descriptor.context_count
            != marc::context::internal::lzss_field_context_count
        || descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes
        || layout_.frequency_entries > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit
               > limits.max_range_model_total) {
        return fail(ContextualDynamicRangeDecodeError::invalid_descriptor);
    }
    if (payload.size() != descriptor.payload_size) {
        return fail(
            ContextualDynamicRangeDecodeError::payload_size_mismatch);
    }

    payload_ = payload;
    descriptor_ = descriptor;
    for (int index = 0; index < 5; ++index) {
        if (payload_offset_ >= payload_.size()) {
            return fail(ContextualDynamicRangeDecodeError::truncated_payload);
        }
        const auto byte =
            std::to_integer<std::uint8_t>(payload_[payload_offset_++]);
        if (index == 0 && byte != 0) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        code_ = static_cast<std::uint32_t>((code_ << 8) | byte);
    }
    started_ = true;
    return result();
}

bool ContextualDynamicRangeDecoder::decode_interval(
    const std::uint32_t cumulative, const std::uint16_t frequency,
    const std::uint32_t total) noexcept {
    if (total == 0 || frequency == 0 || cumulative >= total
        || static_cast<std::uint64_t>(cumulative) + frequency > total
        || range_ < normalization_threshold) {
        return false;
    }
    const auto unit = range_ / total;
    if (unit == 0) return false;
    code_ -= cumulative * unit;
    range_ = unit * frequency;
    while (range_ < normalization_threshold) {
        if (payload_offset_ >= payload_.size()) return false;
        const auto byte =
            std::to_integer<std::uint8_t>(payload_[payload_offset_++]);
        range_ <<= 8;
        code_ = static_cast<std::uint32_t>((code_ << 8) | byte);
    }
    return true;
}

ContextualDynamicRangeDecodeResult
ContextualDynamicRangeDecoder::decode_symbol(
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualDynamicRangeDecodeError::none
            || error_ == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualDynamicRangeDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (expected_context
        >= marc::context::internal::lzss_field_context_count) {
        return fail(ContextualDynamicRangeDecodeError::invalid_context);
    }
    if (expected_alphabet
        != (*layout_.alphabets)[expected_context]) {
        return fail(ContextualDynamicRangeDecodeError::invalid_alphabet);
    }
    if (decision_count_ >= descriptor_.decision_count) {
        return fail(
            ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }

    const auto offset =
        (*layout_.offsets)[expected_context];
    const auto total = totals_[expected_context];
    const auto unit = range_ / total;
    if (range_ < normalization_threshold || unit == 0) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    const auto scaled = code_ / unit;
    if (scaled >= total) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    std::uint32_t cumulative{};
    std::uint32_t decoded{};
    for (; decoded < expected_alphabet; ++decoded) {
        const auto frequency = frequencies_[offset + decoded];
        if (scaled < cumulative + frequency) break;
        cumulative += frequency;
    }
    if (decoded >= expected_alphabet) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    auto& frequency = frequencies_[offset + decoded];
    if (!decode_interval(cumulative, frequency, total)) {
        return fail(payload_offset_ >= payload_.size()
                        ? ContextualDynamicRangeDecodeError::truncated_payload
                        : ContextualDynamicRangeDecodeError::invalid_interval);
    }
    ++frequency;
    auto& updated_total = totals_[expected_context];
    ++updated_total;
    if (updated_total == contextual_dynamic_range_model_total_limit) {
        updated_total = 0;
        for (std::size_t index = 0; index < expected_alphabet; ++index) {
            auto& current = frequencies_[offset + index];
            current = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(current) + 1U) / 2U);
            updated_total += current;
        }
    }
    ++event_count_;
    ++decision_count_;
    value = decoded;
    return result();
}

ContextualDynamicRangeDecodeResult
ContextualDynamicRangeDecoder::decode_bypass(
    const std::uint8_t expected_bit_count, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualDynamicRangeDecodeError::none
            || error_ == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualDynamicRangeDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (expected_bit_count == 0
        || expected_bit_count > layout_.maximum_bypass_bits) {
        return fail(
            ContextualDynamicRangeDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > descriptor_.decision_count
        || expected_bit_count
               > descriptor_.decision_count - decision_count_) {
        return fail(
            ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }

    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < expected_bit_count;
         ++bit_index) {
        const auto unit = range_ / 2U;
        if (range_ < normalization_threshold || unit == 0) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        const auto scaled = code_ / unit;
        if (scaled >= 2) {
            return fail(ContextualDynamicRangeDecodeError::invalid_interval);
        }
        const auto bit = static_cast<std::uint32_t>(scaled);
        if (!decode_interval(bit, 1, 2)) {
            return fail(payload_offset_ >= payload_.size()
                            ? ContextualDynamicRangeDecodeError::truncated_payload
                            : ContextualDynamicRangeDecodeError::invalid_interval);
        }
        decoded |= bit << bit_index;
        ++decision_count_;
    }
    ++event_count_;
    value = decoded;
    return result();
}

bool ContextualDynamicRangeDecoder::validate_models() const noexcept {
    for (std::size_t context = 0; context < totals_.size(); ++context) {
        const auto alphabet =
            (*layout_.alphabets)[context];
        if (totals_[context] < alphabet
            || totals_[context] >= contextual_dynamic_range_model_total_limit) {
            return false;
        }
        std::uint32_t sum{};
        for (std::size_t symbol = 0; symbol < alphabet; ++symbol) {
            const auto frequency =
                frequencies_[(*layout_.offsets)[context]
                             + symbol];
            if (frequency == 0) return false;
            sum += frequency;
        }
        if (sum != totals_[context]) return false;
    }
    return true;
}

ContextualDynamicRangeDecodeResult ContextualDynamicRangeDecoder::finish(
    const std::uint32_t expected_event_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (!started_) {
        if (error_ == ContextualDynamicRangeDecodeError::none
            || error_ == ContextualDynamicRangeDecodeError::not_started) {
            return fail(ContextualDynamicRangeDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualDynamicRangeDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualDynamicRangeDecodeError::already_finished);
    }
    if (event_count_ != expected_event_count
        || decision_count_ != expected_decision_count
        || decision_count_ != descriptor_.decision_count) {
        return fail(ContextualDynamicRangeDecodeError::count_mismatch);
    }
    if (payload_offset_ != payload_.size()) {
        return fail(ContextualDynamicRangeDecodeError::trailing_payload);
    }
    if (!validate_models()) {
        return fail(ContextualDynamicRangeDecodeError::invalid_model);
    }
    finished_ = true;
    return result();
}

} // namespace marc::entropy::internal

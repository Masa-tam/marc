#include "entropy/lzss_short_match_range_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

} // namespace

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::result()
    const noexcept {
    return {event_count_, decision_count_, payload_offset_, error_};
}

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::fail(
    const ContextualDynamicRangeDecodeError error) noexcept {
    error_ = error;
    return result();
}

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::begin(
    const ContextualDynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
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
    frequencies_.fill(1);
    for (std::size_t index = 0; index < totals_.size(); ++index) {
        totals_[index] = context::internal::lzss_short_match_alphabets[index];
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count == 0 || descriptor.payload_size < 5
        || descriptor.context_count
               != context::internal::lzss_short_match_context_count
        || descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes
        || context::internal::lzss_short_match_frequency_entries
               > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit
               > limits.max_range_model_total) {
        return fail(ContextualDynamicRangeDecodeError::invalid_descriptor);
    }
    if (payload.size() != descriptor.payload_size) {
        return fail(ContextualDynamicRangeDecodeError::payload_size_mismatch);
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

bool LzssShortMatchRangeDecoder::decode_interval(
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

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::decode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    std::uint32_t& value) noexcept {
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
    if (context_id >= context::internal::lzss_short_match_context_count) {
        return fail(ContextualDynamicRangeDecodeError::invalid_context);
    }
    if (alphabet != context::internal::lzss_short_match_alphabets[context_id]) {
        return fail(ContextualDynamicRangeDecodeError::invalid_alphabet);
    }
    if (decision_count_ >= descriptor_.decision_count) {
        return fail(ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }
    const auto offset = context::internal::lzss_short_match_offsets[context_id];
    const auto total = totals_[context_id];
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
    for (; decoded < alphabet; ++decoded) {
        const auto frequency = frequencies_[offset + decoded];
        if (scaled < cumulative + frequency) break;
        cumulative += frequency;
    }
    if (decoded >= alphabet) {
        return fail(ContextualDynamicRangeDecodeError::invalid_interval);
    }
    auto& frequency = frequencies_[offset + decoded];
    if (!decode_interval(cumulative, frequency, total)) {
        return fail(payload_offset_ >= payload_.size()
                        ? ContextualDynamicRangeDecodeError::truncated_payload
                        : ContextualDynamicRangeDecodeError::invalid_interval);
    }
    ++frequency;
    auto& updated_total = totals_[context_id];
    ++updated_total;
    if (updated_total == contextual_dynamic_range_model_total_limit) {
        updated_total = 0;
        for (std::size_t index = 0; index < alphabet; ++index) {
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

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::decode_bypass(
    const std::uint8_t bit_count, std::uint32_t& value) noexcept {
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
    if (bit_count == 0 || bit_count > 16) {
        return fail(ContextualDynamicRangeDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > descriptor_.decision_count
        || bit_count > descriptor_.decision_count - decision_count_) {
        return fail(ContextualDynamicRangeDecodeError::decision_count_exceeded);
    }
    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < bit_count; ++bit_index) {
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

bool LzssShortMatchRangeDecoder::validate_models() const noexcept {
    for (std::size_t context_id = 0; context_id < totals_.size(); ++context_id) {
        const auto alphabet =
            context::internal::lzss_short_match_alphabets[context_id];
        if (totals_[context_id] < alphabet
            || totals_[context_id] >= contextual_dynamic_range_model_total_limit) {
            return false;
        }
        std::uint32_t sum{};
        for (std::size_t symbol = 0; symbol < alphabet; ++symbol) {
            const auto frequency = frequencies_[
                context::internal::lzss_short_match_offsets[context_id] + symbol];
            if (frequency == 0) return false;
            sum += frequency;
        }
        if (sum != totals_[context_id]) return false;
    }
    return true;
}

ContextualDynamicRangeDecodeResult LzssShortMatchRangeDecoder::finish(
    const std::uint32_t event_count,
    const std::uint32_t decision_count) noexcept {
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
    if (event_count_ != event_count || decision_count_ != decision_count
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

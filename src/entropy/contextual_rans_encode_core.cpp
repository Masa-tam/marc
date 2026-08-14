#include "entropy/contextual_rans_encode_core.hpp"

#include "context/lzss_field_context_format.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] std::int64_t normalization_error(
    const std::uint32_t count, const std::uint16_t frequency,
    const std::uint32_t total) noexcept {
    return static_cast<std::int64_t>(count)
            * contextual_rans_total_frequency
        - static_cast<std::int64_t>(frequency) * total;
}

[[nodiscard]] bool normalize_context(
    const std::uint16_t context_id,
    const std::array<std::uint32_t, contextual_rans_frequency_entries>& counts,
    const std::array<std::uint32_t, contextual_rans_context_count>& totals,
    ContextualRansFrequencies& frequencies)
    noexcept {
    const auto total = totals[context_id];
    if (total == 0) return true;
    const auto begin =
        context::internal::lzss_field_context_offsets[context_id];
    const auto end =
        context::internal::lzss_field_context_offsets[context_id + 1];
    std::uint32_t sum{};
    for (auto index = begin; index < end; ++index) {
        if (counts[index] == 0) continue;
        const auto scaled = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(counts[index])
             * contextual_rans_total_frequency) / total);
        frequencies[index] = static_cast<std::uint16_t>(
            scaled == 0 ? 1 : scaled);
        sum += frequencies[index];
    }
    while (sum < contextual_rans_total_frequency) {
        std::size_t selected{};
        std::int64_t best = std::numeric_limits<std::int64_t>::min();
        bool found{};
        for (auto index = begin; index < end; ++index) {
            if (counts[index] == 0) continue;
            const auto error = normalization_error(
                counts[index], frequencies[index], total);
            if (!found || error > best) {
                selected = index;
                best = error;
                found = true;
            }
        }
        if (!found || frequencies[selected] == UINT16_MAX) return false;
        ++frequencies[selected];
        ++sum;
    }
    while (sum > contextual_rans_total_frequency) {
        std::size_t selected{};
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        bool found{};
        for (auto index = begin; index < end; ++index) {
            if (frequencies[index] <= 1) continue;
            const auto error = normalization_error(
                counts[index], frequencies[index], total);
            if (!found || error < best
                || (error == best && index > selected)) {
                selected = index;
                best = error;
                found = true;
            }
        }
        if (!found) return false;
        --frequencies[selected];
        --sum;
    }
    return true;
}

[[nodiscard]] std::uint16_t cumulative_for(
    const ContextualRansDescriptor& descriptor,
    const std::uint16_t context_id, const std::uint32_t value) noexcept {
    const auto offset =
        context::internal::lzss_field_context_offsets[context_id];
    std::uint32_t cumulative{};
    for (std::uint32_t symbol = 0; symbol < value; ++symbol) {
        cumulative += descriptor.frequencies[offset + symbol];
    }
    return static_cast<std::uint16_t>(cumulative);
}

} // namespace

ContextualRansEncodeError ContextualRansModelBuilder::add_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value) noexcept {
    if (context_id >= contextual_rans_context_count) {
        return ContextualRansEncodeError::invalid_context;
    }
    if (alphabet
        != context::internal::lzss_field_context_alphabets[context_id]) {
        return ContextualRansEncodeError::invalid_alphabet;
    }
    if (value >= alphabet) return ContextualRansEncodeError::invalid_symbol;
    const auto index =
        context::internal::lzss_field_context_offsets[context_id] + value;
    if (counts_[index] == UINT32_MAX || totals_[context_id] == UINT32_MAX
        || decision_count_ == UINT32_MAX) {
        return ContextualRansEncodeError::arithmetic_overflow;
    }
    ++counts_[index];
    ++totals_[context_id];
    ++decision_count_;
    return ContextualRansEncodeError::none;
}

ContextualRansEncodeError ContextualRansModelBuilder::add_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (bit_count == 0 || bit_count > 16) {
        return ContextualRansEncodeError::invalid_bypass_width;
    }
    if ((value >> bit_count) != 0) {
        return ContextualRansEncodeError::nonzero_unused_field;
    }
    std::uint32_t updated{};
    if (!core::checked_add(
            decision_count_, static_cast<std::uint32_t>(bit_count), updated)) {
        return ContextualRansEncodeError::arithmetic_overflow;
    }
    decision_count_ = updated;
    return ContextualRansEncodeError::none;
}

ContextualRansEncodeError ContextualRansModelBuilder::finish(
    ContextualRansDescriptor& descriptor) const noexcept {
    if (decision_count_ == 0) {
        return ContextualRansEncodeError::empty_operations;
    }
    ContextualRansDescriptor planned{};
    planned.decision_count = decision_count_;
    for (std::uint16_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        if (!normalize_context(
                context_id, counts_, totals_, planned.frequencies)) {
            return ContextualRansEncodeError::normalization_error;
        }
    }
    descriptor = planned;
    return ContextualRansEncodeError::none;
}

ContextualRansReverseWriter::ContextualRansReverseWriter(
    const ContextualRansDescriptor& descriptor,
    const std::span<std::byte> output) noexcept
    : descriptor_(descriptor), output_(output), cursor_(output.size()) {}

ContextualRansEncodeError ContextualRansReverseWriter::encode_range(
    const std::uint16_t cumulative, const std::uint16_t frequency) noexcept {
    if (error_ != ContextualRansEncodeError::none || finished_) {
        error_ = ContextualRansEncodeError::internal_error;
        return error_;
    }
    if (frequency == 0
        || static_cast<std::uint32_t>(cumulative) + frequency
               > contextual_rans_total_frequency
        || state_ < rans_lower_bound
        || state_ >= rans_lower_bound * UINT64_C(256)) {
        error_ = ContextualRansEncodeError::internal_error;
        return error_;
    }
    const auto maximum =
        ((rans_lower_bound >> contextual_rans_table_log) << 8) * frequency;
    while (state_ >= maximum) {
        if (renormalization_bytes_
            == std::numeric_limits<std::size_t>::max()) {
            error_ = ContextualRansEncodeError::arithmetic_overflow;
            return error_;
        }
        if (!output_.empty()) {
            if (cursor_ <= rans_min_payload_size) {
                error_ = ContextualRansEncodeError::internal_error;
                return error_;
            }
            output_[--cursor_] = static_cast<std::byte>(state_ & 0xffU);
        }
        ++renormalization_bytes_;
        state_ >>= 8;
    }
    std::uint64_t quotient_part{};
    if (!core::checked_multiply(
            state_ / frequency,
            static_cast<std::uint64_t>(contextual_rans_total_frequency),
            quotient_part)
        || !core::checked_add(quotient_part, state_ % frequency, state_)
        || !core::checked_add(
            state_, static_cast<std::uint64_t>(cumulative), state_)) {
        error_ = ContextualRansEncodeError::arithmetic_overflow;
    }
    return error_;
}

ContextualRansEncodeError ContextualRansReverseWriter::encode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value) noexcept {
    if (context_id >= contextual_rans_context_count) {
        return error_ = ContextualRansEncodeError::invalid_context;
    }
    if (alphabet
        != context::internal::lzss_field_context_alphabets[context_id]) {
        return error_ = ContextualRansEncodeError::invalid_alphabet;
    }
    if (value >= alphabet) {
        return error_ = ContextualRansEncodeError::invalid_symbol;
    }
    const auto offset =
        context::internal::lzss_field_context_offsets[context_id];
    return encode_range(
        cumulative_for(descriptor_, context_id, value),
        descriptor_.frequencies[offset + value]);
}

ContextualRansEncodeError ContextualRansReverseWriter::encode_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (bit_count == 0 || bit_count > 16) {
        return error_ = ContextualRansEncodeError::invalid_bypass_width;
    }
    if ((value >> bit_count) != 0) {
        return error_ = ContextualRansEncodeError::nonzero_unused_field;
    }
    for (std::uint8_t reverse_bit = bit_count; reverse_bit != 0;
         --reverse_bit) {
        const auto bit = static_cast<std::uint16_t>(
            (value >> (reverse_bit - 1)) & 1U);
        const auto error = encode_range(
            static_cast<std::uint16_t>(
                bit * (contextual_rans_total_frequency / 2U)),
            static_cast<std::uint16_t>(
                contextual_rans_total_frequency / 2U));
        if (error != ContextualRansEncodeError::none) return error;
    }
    return ContextualRansEncodeError::none;
}

ContextualRansEncodeError ContextualRansReverseWriter::finish(
    std::size_t& payload_size) noexcept {
    if (error_ != ContextualRansEncodeError::none) return error_;
    if (finished_ || state_ < rans_lower_bound
        || state_ >= rans_lower_bound * UINT64_C(256)) {
        return error_ = ContextualRansEncodeError::internal_error;
    }
    std::size_t size{};
    if (!core::checked_add(
            static_cast<std::size_t>(rans_min_payload_size),
            renormalization_bytes_, size)) {
        return error_ = ContextualRansEncodeError::arithmetic_overflow;
    }
    if (!output_.empty()
        && (output_.size() != size || cursor_ != rans_min_payload_size
            || !core::store_le(output_, 0, state_))) {
        return error_ = ContextualRansEncodeError::internal_error;
    }
    payload_size = size;
    finished_ = true;
    return ContextualRansEncodeError::none;
}

} // namespace marc::entropy::internal

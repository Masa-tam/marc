#include "entropy/contextual_tans_encode_core.hpp"

#include "context/lzss_field_context_format.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/tans_tables.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] std::int64_t normalization_error(
    const std::uint32_t count, const std::uint16_t frequency,
    const std::uint32_t total) noexcept {
    return static_cast<std::int64_t>(count)
            * contextual_tans_total_frequency
        - static_cast<std::int64_t>(frequency) * total;
}

[[nodiscard]] bool normalize_context(
    const std::uint16_t context_id,
    const std::array<std::uint32_t, contextual_tans_frequency_entries>& counts,
    const std::array<std::uint32_t, contextual_tans_context_count>& totals,
    ContextualCompactFrequencies& frequencies) noexcept {
    const auto total = totals[context_id];
    if (total == 0) return true;
    const auto begin = context::internal::lzss_field_context_offsets[context_id];
    const auto end = context::internal::lzss_field_context_offsets[context_id + 1];
    std::uint32_t sum{};
    for (auto index = begin; index < end; ++index) {
        if (counts[index] == 0) continue;
        const auto scaled = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(counts[index])
             * contextual_tans_total_frequency) / total);
        frequencies[index] = static_cast<std::uint16_t>(
            scaled == 0 ? 1 : scaled);
        sum += frequencies[index];
    }
    while (sum < contextual_tans_total_frequency) {
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
    while (sum > contextual_tans_total_frequency) {
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

[[nodiscard]] TansDescriptor context_descriptor(
    const ContextualTansDescriptor& descriptor,
    const std::size_t context_id) noexcept {
    TansDescriptor table{};
    const auto begin = context::internal::lzss_field_context_offsets[context_id];
    const auto alphabet =
        context::internal::lzss_field_context_alphabets[context_id];
    std::copy_n(
        descriptor.frequencies.begin() + begin, alphabet,
        table.frequencies.begin());
    return table;
}

[[nodiscard]] TansDescriptor bypass_descriptor() noexcept {
    TansDescriptor table{};
    table.frequencies[0] = static_cast<std::uint16_t>(
        contextual_tans_total_frequency / 2);
    table.frequencies[1] = static_cast<std::uint16_t>(
        contextual_tans_total_frequency / 2);
    return table;
}

[[nodiscard]] std::uint16_t cumulative_for(
    const ContextualTansDescriptor& descriptor,
    const std::uint16_t context_id,
    const std::uint32_t value) noexcept {
    const auto offset = context::internal::lzss_field_context_offsets[context_id];
    std::uint32_t cumulative{};
    for (std::uint32_t symbol = 0; symbol < value; ++symbol) {
        cumulative += descriptor.frequencies[offset + symbol];
    }
    return static_cast<std::uint16_t>(cumulative);
}

} // namespace

ContextualTansEncodeError ContextualTansModelBuilder::add_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value) noexcept {
    if (context_id >= contextual_tans_context_count) {
        return ContextualTansEncodeError::invalid_context;
    }
    if (alphabet
        != context::internal::lzss_field_context_alphabets[context_id]) {
        return ContextualTansEncodeError::invalid_alphabet;
    }
    if (value >= alphabet) return ContextualTansEncodeError::invalid_symbol;
    const auto index =
        context::internal::lzss_field_context_offsets[context_id] + value;
    if (counts_[index] == UINT32_MAX || totals_[context_id] == UINT32_MAX
        || decision_count_ == UINT32_MAX) {
        return ContextualTansEncodeError::arithmetic_overflow;
    }
    ++counts_[index];
    ++totals_[context_id];
    ++decision_count_;
    return ContextualTansEncodeError::none;
}

ContextualTansEncodeError ContextualTansModelBuilder::add_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (bit_count == 0 || bit_count > 16) {
        return ContextualTansEncodeError::invalid_bypass_width;
    }
    if ((value >> bit_count) != 0) {
        return ContextualTansEncodeError::nonzero_unused_field;
    }
    std::uint32_t updated{};
    if (!core::checked_add(
            decision_count_, static_cast<std::uint32_t>(bit_count), updated)) {
        return ContextualTansEncodeError::arithmetic_overflow;
    }
    decision_count_ = updated;
    return ContextualTansEncodeError::none;
}

ContextualTansEncodeError ContextualTansModelBuilder::finish(
    ContextualTansDescriptor& descriptor) const noexcept {
    if (decision_count_ == 0) {
        return ContextualTansEncodeError::empty_operations;
    }
    ContextualTansDescriptor planned{};
    planned.decision_count = decision_count_;
    planned.payload_size = static_cast<std::uint32_t>(tans_min_payload_size);
    for (std::uint16_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        if (!normalize_context(
                context_id, counts_, totals_, planned.frequencies)) {
            return ContextualTansEncodeError::normalization_error;
        }
    }
    descriptor = planned;
    return ContextualTansEncodeError::none;
}

ContextualTansEncodeError build_contextual_tans_encode_tables(
    const ContextualTansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    const std::span<std::uint16_t> output) noexcept {
    const auto snapshot = descriptor;
    std::size_t descriptor_size{};
    const auto format_error = validate_contextual_tans_descriptor(
        snapshot, snapshot.decision_count, snapshot.payload_size, limits,
        descriptor_size);
    if (format_error == ContextualTansFormatError::limit_exceeded) {
        return ContextualTansEncodeError::limit_exceeded;
    }
    if (format_error == ContextualTansFormatError::arithmetic_overflow) {
        return ContextualTansEncodeError::arithmetic_overflow;
    }
    if (format_error != ContextualTansFormatError::none) {
        return ContextualTansEncodeError::table_error;
    }
    if (output.size() < contextual_tans_encode_table_entries) {
        return ContextualTansEncodeError::table_output_too_small;
    }
    TansTables scratch{};
    for (std::size_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        const auto begin =
            context::internal::lzss_field_context_offsets[context_id];
        const auto end =
            context::internal::lzss_field_context_offsets[context_id + 1];
        const bool active = std::any_of(
            snapshot.frequencies.begin() + begin,
            snapshot.frequencies.begin() + end,
            [](const auto frequency) { return frequency != 0; });
        if (!active) continue;
        if (build_tans_tables(context_descriptor(snapshot, context_id),
                              scratch) != TansTableError::none) {
            return ContextualTansEncodeError::table_error;
        }
    }
    if (build_tans_tables(bypass_descriptor(), scratch)
        != TansTableError::none) {
        return ContextualTansEncodeError::table_error;
    }

    auto used = output.first(contextual_tans_encode_table_entries);
    std::fill(used.begin(), used.end(), std::uint16_t{});
    for (std::size_t context_id = 0;
         context_id < contextual_tans_context_count; ++context_id) {
        const auto begin =
            context::internal::lzss_field_context_offsets[context_id];
        const auto end =
            context::internal::lzss_field_context_offsets[context_id + 1];
        const bool active = std::any_of(
            snapshot.frequencies.begin() + begin,
            snapshot.frequencies.begin() + end,
            [](const auto frequency) { return frequency != 0; });
        if (!active) continue;
        const auto ignored = build_tans_tables(
            context_descriptor(snapshot, context_id), scratch);
        (void)ignored;
        std::copy(
            scratch.encode_states.begin(), scratch.encode_states.end(),
            used.begin() + context_id * contextual_tans_total_frequency);
    }
    const auto ignored = build_tans_tables(bypass_descriptor(), scratch);
    (void)ignored;
    std::copy(
        scratch.encode_states.begin(), scratch.encode_states.end(),
        used.begin() + contextual_tans_context_count
            * contextual_tans_total_frequency);
    return ContextualTansEncodeError::none;
}

ContextualTansReverseWriter::ContextualTansReverseWriter(
    const ContextualTansDescriptor& descriptor,
    const std::span<const std::uint16_t> encode_tables,
    const std::span<std::byte> output) noexcept
    : descriptor_(descriptor), encode_tables_(encode_tables), output_(output) {
    if (!output_.empty()) {
        std::size_t total_bits{};
        if (descriptor_.payload_size > tans_min_payload_size
            && (!core::checked_multiply(
                    static_cast<std::size_t>(descriptor_.payload_size)
                        - tans_min_payload_size - 1,
                    std::size_t{8}, total_bits)
                || !core::checked_add(
                    total_bits,
                    static_cast<std::size_t>(descriptor_.final_valid_bits),
                    total_bits))) {
            error_ = ContextualTansEncodeError::arithmetic_overflow;
            return;
        }
        cursor_ = total_bits;
    }
}

ContextualTansEncodeError ContextualTansReverseWriter::encode_transition(
    const std::size_t table_index, const std::uint16_t cumulative,
    const std::uint16_t frequency) noexcept {
    if (error_ != ContextualTansEncodeError::none || finished_
        || frequency == 0
        || static_cast<std::uint32_t>(cumulative) + frequency
               > contextual_tans_total_frequency
        || table_index > contextual_tans_context_count
        || encode_tables_.size() < contextual_tans_encode_table_entries
        || state_ < contextual_tans_total_frequency
        || state_ >= 2U * contextual_tans_total_frequency) {
        return error_ = ContextualTansEncodeError::internal_error;
    }
    bool found{};
    std::uint16_t next{};
    std::uint8_t selected_bits{};
    for (std::uint8_t bits = 0; bits <= contextual_tans_table_log; ++bits) {
        const auto reduced = state_ >> bits;
        if (reduced < frequency || reduced >= 2U * frequency) continue;
        const auto local_index = static_cast<std::uint32_t>(cumulative)
            + reduced - frequency;
        if (local_index >= contextual_tans_total_frequency || found) {
            return error_ = ContextualTansEncodeError::internal_error;
        }
        const auto candidate = encode_tables_[
            table_index * contextual_tans_total_frequency + local_index];
        if (candidate < contextual_tans_total_frequency
            || candidate >= 2U * contextual_tans_total_frequency) {
            return error_ = ContextualTansEncodeError::internal_error;
        }
        found = true;
        next = candidate;
        selected_bits = bits;
    }
    if (!found) return error_ = ContextualTansEncodeError::internal_error;
    if (!core::checked_add(
            bit_count_, static_cast<std::size_t>(selected_bits), bit_count_)) {
        return error_ = ContextualTansEncodeError::arithmetic_overflow;
    }
    if (!output_.empty()) {
        if (cursor_ < selected_bits) {
            return error_ = ContextualTansEncodeError::internal_error;
        }
        cursor_ -= selected_bits;
        const auto mask = selected_bits == 0
            ? UINT32_C(0)
            : (UINT32_C(1) << selected_bits) - 1;
        const auto value = state_ & mask;
        for (std::uint8_t bit = 0; bit < selected_bits; ++bit) {
            const auto position = cursor_ + bit;
            if (((value >> bit) & 1U) != 0) {
                output_[tans_min_payload_size + position / 8]
                    |= static_cast<std::byte>(UINT8_C(1) << (position % 8));
            }
        }
    }
    state_ = next;
    return error_;
}

ContextualTansEncodeError ContextualTansReverseWriter::encode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet,
    const std::uint32_t value) noexcept {
    if (context_id >= contextual_tans_context_count) {
        return error_ = ContextualTansEncodeError::invalid_context;
    }
    if (alphabet
        != context::internal::lzss_field_context_alphabets[context_id]) {
        return error_ = ContextualTansEncodeError::invalid_alphabet;
    }
    if (value >= alphabet) {
        return error_ = ContextualTansEncodeError::invalid_symbol;
    }
    const auto offset = context::internal::lzss_field_context_offsets[context_id];
    return encode_transition(
        context_id, cumulative_for(descriptor_, context_id, value),
        descriptor_.frequencies[offset + value]);
}

ContextualTansEncodeError ContextualTansReverseWriter::encode_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (bit_count == 0 || bit_count > 16) {
        return error_ = ContextualTansEncodeError::invalid_bypass_width;
    }
    if ((value >> bit_count) != 0) {
        return error_ = ContextualTansEncodeError::nonzero_unused_field;
    }
    constexpr auto half = static_cast<std::uint16_t>(
        contextual_tans_total_frequency / 2);
    for (std::uint8_t reverse_bit = bit_count; reverse_bit != 0;
         --reverse_bit) {
        const auto bit = static_cast<std::uint16_t>(
            (value >> (reverse_bit - 1)) & 1U);
        const auto error = encode_transition(
            contextual_tans_context_count,
            static_cast<std::uint16_t>(bit * half), half);
        if (error != ContextualTansEncodeError::none) return error;
    }
    return ContextualTansEncodeError::none;
}

ContextualTansEncodeError ContextualTansReverseWriter::finish(
    std::size_t& payload_size, std::uint8_t& final_valid_bits) noexcept {
    if (error_ != ContextualTansEncodeError::none) return error_;
    if (finished_ || state_ < contextual_tans_total_frequency
        || state_ >= 2U * contextual_tans_total_frequency) {
        return error_ = ContextualTansEncodeError::internal_error;
    }
    std::size_t rounded{};
    if (!core::checked_add(bit_count_, std::size_t{7}, rounded)
        || !core::checked_add(
            static_cast<std::size_t>(tans_min_payload_size), rounded / 8,
            payload_size)) {
        return error_ = ContextualTansEncodeError::arithmetic_overflow;
    }
    final_valid_bits = bit_count_ == 0 ? 0
        : static_cast<std::uint8_t>((bit_count_ - 1) % 8 + 1);
    if (!output_.empty()
        && (output_.size() != payload_size || cursor_ != 0
            || !core::store_le(
                output_, 0, static_cast<std::uint16_t>(
                    state_ - contextual_tans_total_frequency)))) {
        return error_ = ContextualTansEncodeError::internal_error;
    }
    finished_ = true;
    return ContextualTansEncodeError::none;
}

} // namespace marc::entropy::internal

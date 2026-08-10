#include "entropy/contextual_blocked_huffman_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck ranges_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

[[nodiscard]] bool is_table_model(
    const ContextualBlockedHuffmanModel& model) noexcept {
    return model.active
        && model.single_symbol
               == contextual_blocked_huffman_no_single_symbol;
}

} // namespace

void ContextualBlockedHuffmanDecoder::reset() noexcept {
    payload_ = {};
    tables_ = {};
    field_models_ = {};
    override_models_ = {};
    requested_overrides_.fill(false);
    override_mask_ = 0;
    total_bits_ = 0;
    bit_offset_ = 0;
    expected_decisions_ = 0;
    event_count_ = 0;
    decision_count_ = 0;
    error_ = ContextualBlockedHuffmanDecodeError::none;
    started_ = false;
    finished_ = false;
}

ContextualBlockedHuffmanDecodeResult
ContextualBlockedHuffmanDecoder::result() const noexcept {
    return {event_count_, decision_count_, bit_offset_, error_};
}

ContextualBlockedHuffmanDecodeResult ContextualBlockedHuffmanDecoder::fail(
    const ContextualBlockedHuffmanDecodeError error) noexcept {
    error_ = error;
    return result();
}

ContextualBlockedHuffmanDecodeResult ContextualBlockedHuffmanDecoder::begin(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<HuffmanDecodeTable> table_output) noexcept {
    reset();
    if (payload.size() != descriptor.payload_size) {
        return fail(
            ContextualBlockedHuffmanDecodeError::payload_size_mismatch);
    }
    std::size_t descriptor_size{};
    if (validate_contextual_blocked_huffman_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            limits, descriptor_size)
        != ContextualBlockedHuffmanFormatError::none) {
        return fail(ContextualBlockedHuffmanDecodeError::invalid_descriptor);
    }

    std::size_t total_bits{};
    if (!payload.empty()) {
        if (!core::checked_multiply(
                payload.size() - 1U, std::size_t{8}, total_bits)
            || !core::checked_add(
                total_bits,
                static_cast<std::size_t>(descriptor.final_valid_bits),
                total_bits)) {
            return fail(
                ContextualBlockedHuffmanDecodeError::arithmetic_overflow);
        }
        if (descriptor.final_valid_bits < 8) {
            const auto high_mask = static_cast<std::uint8_t>(
                0xffU << descriptor.final_valid_bits);
            if ((std::to_integer<std::uint8_t>(payload.back()) & high_mask)
                != 0) {
                return fail(
                    ContextualBlockedHuffmanDecodeError::nonzero_padding);
            }
        }
    }

    std::size_t required_tables{};
    for (const auto& model : descriptor.field_models) {
        if (is_table_model(model)) ++required_tables;
    }
    for (const auto& model : descriptor.context_models) {
        if (is_table_model(model)) ++required_tables;
    }
    if (table_output.size() < required_tables) {
        return fail(
            ContextualBlockedHuffmanDecodeError::table_output_too_small);
    }
    std::size_t table_bytes{};
    if (!core::checked_multiply(
            required_tables, sizeof(HuffmanDecodeTable), table_bytes)) {
        return fail(ContextualBlockedHuffmanDecodeError::arithmetic_overflow);
    }
    const auto overlap = ranges_overlap(
        payload.data(), payload.size(), table_output.data(), table_bytes);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        return fail(ContextualBlockedHuffmanDecodeError::arithmetic_overflow);
    }
    if (overlap == OverlapCheck::overlap) {
        return fail(ContextualBlockedHuffmanDecodeError::overlapping_buffers);
    }

    std::array<ModelSelection,
               contextual_blocked_huffman_field_table_count>
        fields{};
    std::array<ModelSelection,
               context::internal::lzss_field_context_count>
        overrides{};
    std::size_t table_index{};
    const auto build = [&](const ContextualBlockedHuffmanModel& source,
                           ModelSelection& target) noexcept {
        if (!source.active) return ContextualBlockedHuffmanDecodeError::none;
        target.active = true;
        target.single_symbol = source.single_symbol;
        if (!is_table_model(source)) {
            return ContextualBlockedHuffmanDecodeError::none;
        }
        if (table_index >= required_tables || table_index > UINT8_MAX) {
            return ContextualBlockedHuffmanDecodeError::internal_error;
        }
        if (build_decode_table(source.lengths, table_output[table_index])
            != HuffmanTableError::none) {
            return ContextualBlockedHuffmanDecodeError::invalid_table;
        }
        target.table_index = static_cast<std::uint8_t>(table_index++);
        return ContextualBlockedHuffmanDecodeError::none;
    };
    for (std::size_t field = 0; field < fields.size(); ++field) {
        const auto error = build(descriptor.field_models[field], fields[field]);
        if (error != ContextualBlockedHuffmanDecodeError::none) {
            return fail(error);
        }
    }
    for (std::size_t context_id = 0;
         context_id < overrides.size(); ++context_id) {
        const auto error = build(
            descriptor.context_models[context_id], overrides[context_id]);
        if (error != ContextualBlockedHuffmanDecodeError::none) {
            return fail(error);
        }
    }
    if (table_index != required_tables) {
        return fail(ContextualBlockedHuffmanDecodeError::internal_error);
    }

    payload_ = payload;
    tables_ = table_output.first(required_tables);
    field_models_ = fields;
    override_models_ = overrides;
    override_mask_ = descriptor.override_mask;
    total_bits_ = total_bits;
    expected_decisions_ = descriptor.decision_count;
    started_ = true;
    return result();
}

ContextualBlockedHuffmanDecodeError
ContextualBlockedHuffmanDecoder::decode_model(
    const ModelSelection& model, std::uint32_t& value) noexcept {
    if (!model.active) {
        return ContextualBlockedHuffmanDecodeError::inactive_context;
    }
    if (model.single_symbol
        != contextual_blocked_huffman_no_single_symbol) {
        value = model.single_symbol;
        return ContextualBlockedHuffmanDecodeError::none;
    }
    if (model.table_index >= tables_.size() || bit_offset_ > total_bits_) {
        return ContextualBlockedHuffmanDecodeError::internal_error;
    }
    const auto remaining = total_bits_ - bit_offset_;
    const auto available = static_cast<std::uint8_t>(
        std::min<std::size_t>(huffman_max_code_length, remaining));
    std::uint16_t bits{};
    for (std::uint8_t bit = 0; bit < available; ++bit) {
        const auto source_offset = bit_offset_ + bit;
        const auto byte = std::to_integer<std::uint8_t>(
            payload_[source_offset / 8U]);
        bits |= static_cast<std::uint16_t>(
            ((byte >> (source_offset % 8U)) & 1U) << bit);
    }
    const auto decoded = marc::entropy::internal::decode_symbol(
        tables_[model.table_index], bits, available);
    if (decoded.status == HuffmanDecodeStatus::need_input) {
        return ContextualBlockedHuffmanDecodeError::truncated_bits;
    }
    if (decoded.status == HuffmanDecodeStatus::invalid_code) {
        return ContextualBlockedHuffmanDecodeError::invalid_code;
    }
    if (decoded.status != HuffmanDecodeStatus::symbol
        || decoded.bits_consumed == 0
        || decoded.bits_consumed > remaining) {
        return ContextualBlockedHuffmanDecodeError::internal_error;
    }
    bit_offset_ += decoded.bits_consumed;
    value = decoded.symbol;
    return ContextualBlockedHuffmanDecodeError::none;
}

ContextualBlockedHuffmanDecodeResult
ContextualBlockedHuffmanDecoder::decode_symbol(
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet,
    std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualBlockedHuffmanDecodeError::none
            || error_ == ContextualBlockedHuffmanDecodeError::not_started) {
            return fail(ContextualBlockedHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualBlockedHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualBlockedHuffmanDecodeError::already_finished);
    }
    if (expected_context
        >= context::internal::lzss_field_context_count) {
        return fail(ContextualBlockedHuffmanDecodeError::invalid_context);
    }
    if (expected_alphabet
        != context::internal::lzss_field_context_alphabets[
            expected_context]) {
        return fail(ContextualBlockedHuffmanDecodeError::invalid_alphabet);
    }
    if (decision_count_ >= expected_decisions_) {
        return fail(
            ContextualBlockedHuffmanDecodeError::decision_count_exceeded);
    }
    const auto field = contextual_blocked_huffman_field_for_context(
        expected_context);
    if (field >= field_models_.size() || !field_models_[field].active) {
        return fail(ContextualBlockedHuffmanDecodeError::inactive_context);
    }
    const bool overridden =
        (override_mask_ & (UINT32_C(1) << expected_context)) != 0;
    const auto& model = overridden
        ? override_models_[expected_context]
        : field_models_[field];
    std::uint32_t decoded{};
    const auto error = decode_model(model, decoded);
    if (error != ContextualBlockedHuffmanDecodeError::none) {
        return fail(error);
    }
    if (overridden) requested_overrides_[expected_context] = true;
    ++event_count_;
    ++decision_count_;
    value = decoded;
    return result();
}

ContextualBlockedHuffmanDecodeResult
ContextualBlockedHuffmanDecoder::decode_bypass(
    const std::uint8_t expected_bit_count, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualBlockedHuffmanDecodeError::none
            || error_ == ContextualBlockedHuffmanDecodeError::not_started) {
            return fail(ContextualBlockedHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualBlockedHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualBlockedHuffmanDecodeError::already_finished);
    }
    if (expected_bit_count == 0 || expected_bit_count > 16) {
        return fail(
            ContextualBlockedHuffmanDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > expected_decisions_
        || expected_bit_count > expected_decisions_ - decision_count_) {
        return fail(
            ContextualBlockedHuffmanDecodeError::decision_count_exceeded);
    }
    if (bit_offset_ > total_bits_
        || expected_bit_count > total_bits_ - bit_offset_) {
        return fail(ContextualBlockedHuffmanDecodeError::truncated_bits);
    }
    std::uint32_t decoded{};
    for (std::uint8_t bit = 0; bit < expected_bit_count; ++bit) {
        const auto source_offset = bit_offset_ + bit;
        const auto byte = std::to_integer<std::uint8_t>(
            payload_[source_offset / 8U]);
        decoded |= static_cast<std::uint32_t>(
            (byte >> (source_offset % 8U)) & 1U) << bit;
    }
    bit_offset_ += expected_bit_count;
    ++event_count_;
    decision_count_ += expected_bit_count;
    value = decoded;
    return result();
}

ContextualBlockedHuffmanDecodeResult
ContextualBlockedHuffmanDecoder::finish(
    const std::uint32_t expected_event_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (!started_) {
        if (error_ == ContextualBlockedHuffmanDecodeError::none
            || error_ == ContextualBlockedHuffmanDecodeError::not_started) {
            return fail(ContextualBlockedHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualBlockedHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualBlockedHuffmanDecodeError::already_finished);
    }
    if (event_count_ != expected_event_count
        || decision_count_ != expected_decision_count
        || decision_count_ != expected_decisions_) {
        return fail(ContextualBlockedHuffmanDecodeError::count_mismatch);
    }
    for (std::size_t context_id = 0;
         context_id < requested_overrides_.size(); ++context_id) {
        if ((override_mask_ & (UINT32_C(1) << context_id)) != 0
            && !requested_overrides_[context_id]) {
            return fail(ContextualBlockedHuffmanDecodeError::unused_override);
        }
    }
    if (bit_offset_ != total_bits_) {
        return fail(ContextualBlockedHuffmanDecodeError::trailing_bits);
    }
    finished_ = true;
    return result();
}

} // namespace marc::entropy::internal

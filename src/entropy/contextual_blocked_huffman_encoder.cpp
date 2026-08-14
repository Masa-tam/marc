#include "entropy/contextual_blocked_huffman_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

struct BuiltModel {
    ContextualBlockedHuffmanModel model{};
    std::uint64_t symbol_bits{};
    std::size_t record_size{};
};

[[nodiscard]] std::array<std::uint16_t, 4> field_alphabets(
    const context::internal::LzssFieldContextLayout& layout) noexcept {
    return {2, 256, 8, (*layout.alphabets)[23]};
}

[[nodiscard]] bool valid_symbol(
    const ModeledOperation& operation,
    const context::internal::LzssFieldContextLayout& layout) noexcept {
    return operation.kind == ModeledOperationKind::symbol
        && operation.bit_count == 0
        && layout.alphabets != nullptr
        && operation.context_id < context::internal::lzss_field_context_count
        && operation.alphabet_size == (*layout.alphabets)[operation.context_id]
        && operation.value < operation.alphabet_size;
}

[[nodiscard]] bool valid_bypass(
    const ModeledOperation& operation,
    const context::internal::LzssFieldContextLayout& layout) noexcept {
    return operation.kind == ModeledOperationKind::bypass_bits
        && operation.context_id == 0 && operation.alphabet_size == 0
        && layout.alphabets != nullptr && operation.bit_count != 0
        && operation.bit_count <= layout.maximum_bypass_bits
        && operation.value < (UINT32_C(1) << operation.bit_count);
}

[[nodiscard]] ContextualBlockedHuffmanEncodeError build_model(
    const HuffmanFrequencies& frequencies, const std::uint16_t alphabet,
    BuiltModel& built) noexcept {
    built = {};
    std::size_t nonzero{};
    std::uint16_t sole{};
    for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
        if (frequencies[symbol] == 0) continue;
        ++nonzero;
        sole = symbol;
    }
    if (nonzero == 0) return ContextualBlockedHuffmanEncodeError::none;
    built.model.active = true;
    if (nonzero == 1) {
        built.model.single_symbol = sole;
        built.record_size = 4;
        return ContextualBlockedHuffmanEncodeError::none;
    }
    if (build_length_limited_code_lengths(
            frequencies, built.model.lengths)
        != HuffmanBuildError::none) {
        return ContextualBlockedHuffmanEncodeError::huffman_build_error;
    }
    for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
        std::uint64_t contribution{};
        if (!core::checked_multiply(
                frequencies[symbol],
                static_cast<std::uint64_t>(built.model.lengths[symbol]),
                contribution)
            || !core::checked_add(
                built.symbol_bits, contribution, built.symbol_bits)) {
            return ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        }
    }
    const auto dense = static_cast<std::size_t>((alphabet + 1U) / 2U);
    const auto sparse = 2U * nonzero;
    built.record_size = 4U + std::min(dense, sparse);
    return ContextualBlockedHuffmanEncodeError::none;
}

[[nodiscard]] std::uint8_t model_length(
    const ContextualBlockedHuffmanModel& model,
    const std::uint16_t symbol) noexcept {
    return model.single_symbol != contextual_blocked_huffman_no_single_symbol
        ? 0
        : model.lengths[symbol];
}

[[nodiscard]] bool ranges_overlap(
    const void* first, const std::size_t first_size, const void* second,
    const std::size_t second_size, bool& overflow) noexcept {
    if (first_size == 0 || second_size == 0) return false;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(first_begin, first_size, first_end)
        || !core::checked_add(second_begin, second_size, second_end)) {
        overflow = true;
        return false;
    }
    return first_begin < second_end && second_begin < first_end;
}

} // namespace

ContextualBlockedHuffmanModelBuilder::ContextualBlockedHuffmanModelBuilder(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    layout_ = selected.layout;
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        error_ = ContextualBlockedHuffmanEncodeError::
            unsupported_context_variant;
    }
}

ContextualBlockedHuffmanEncodeError
ContextualBlockedHuffmanModelBuilder::add_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet_size,
    const std::uint32_t value) noexcept {
    if (error_ != ContextualBlockedHuffmanEncodeError::none) return error_;
    const ModeledOperation operation{
        ModeledOperationKind::symbol, context_id, alphabet_size, value, 0};
    if (!valid_symbol(operation, layout_)) {
        error_ = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return error_;
    }
    auto& context_frequency = context_frequencies_[context_id][value];
    const auto field = contextual_blocked_huffman_field_for_context(context_id);
    auto& field_frequency = field_frequencies_[field][value];
    if (context_frequency == std::numeric_limits<std::uint64_t>::max()
        || field_frequency == std::numeric_limits<std::uint64_t>::max()
        || decision_count_ == std::numeric_limits<std::uint64_t>::max()
        || operation_count_ == std::numeric_limits<std::size_t>::max()) {
        error_ = ContextualBlockedHuffmanEncodeError::frequency_overflow;
        return error_;
    }
    ++context_frequency;
    ++field_frequency;
    ++decision_count_;
    ++operation_count_;
    return error_;
}

ContextualBlockedHuffmanEncodeError
ContextualBlockedHuffmanModelBuilder::add_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (error_ != ContextualBlockedHuffmanEncodeError::none) return error_;
    const ModeledOperation operation{
        ModeledOperationKind::bypass_bits, 0, 0, value, bit_count};
    if (!valid_bypass(operation, layout_)) {
        error_ = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return error_;
    }
    if (!core::checked_add(
            decision_count_, static_cast<std::uint64_t>(bit_count),
            decision_count_)
        || !core::checked_add(
            bypass_bits_, static_cast<std::uint64_t>(bit_count), bypass_bits_)
        || operation_count_ == std::numeric_limits<std::size_t>::max()) {
        error_ = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return error_;
    }
    ++operation_count_;
    return error_;
}

ContextualBlockedHuffmanEncodeResult
ContextualBlockedHuffmanModelBuilder::finish(
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor) const noexcept {
    ContextualBlockedHuffmanEncodeResult result{};
    result.operation_count = operation_count_;
    result.operation_index = operation_count_;
    if (error_ != ContextualBlockedHuffmanEncodeError::none) {
        result.error = error_;
        return result;
    }
    if (decision_count_ == 0 || decision_count_ > UINT32_MAX) {
        result.error = decision_count_ == 0
            ? ContextualBlockedHuffmanEncodeError::invalid_operation
            : ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    ContextualBlockedHuffmanDescriptor planned{};
    planned.decision_count = static_cast<std::uint32_t>(decision_count_);
    const auto selected_field_alphabets = field_alphabets(layout_);
    std::array<BuiltModel, 4> fields{};
    for (std::size_t field = 0; field < fields.size(); ++field) {
        const auto error = build_model(
            field_frequencies_[field], selected_field_alphabets[field],
            fields[field]);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            result.error = error;
            return result;
        }
        if (!fields[field].model.active) continue;
        planned.field_active_mask |= static_cast<std::uint8_t>(1U << field);
        planned.field_models[field] = fields[field].model;
    }
    std::array<BuiltModel, context::internal::lzss_field_context_count>
        contexts{};
    for (std::size_t context_id = 0; context_id < contexts.size(); ++context_id) {
        const auto alphabet = (*layout_.alphabets)[context_id];
        const auto error = build_model(
            context_frequencies_[context_id], alphabet, contexts[context_id]);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            result.error = error;
            return result;
        }
        if (!contexts[context_id].model.active) continue;
        const auto field = contextual_blocked_huffman_field_for_context(
            static_cast<std::uint16_t>(context_id));
        std::uint64_t base_bits{};
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            std::uint64_t contribution{};
            if (!core::checked_multiply(
                    context_frequencies_[context_id][symbol],
                    static_cast<std::uint64_t>(
                        model_length(fields[field].model, symbol)),
                    contribution)
                || !core::checked_add(base_bits, contribution, base_bits)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
        }
        const auto context_bits = contexts[context_id].symbol_bits;
        std::uint64_t record_bits{};
        if (!core::checked_multiply(
                static_cast<std::uint64_t>(contexts[context_id].record_size),
                UINT64_C(8), record_bits)) {
            result.error =
                ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
            return result;
        }
        if (base_bits > context_bits
            && base_bits - context_bits > record_bits) {
            planned.override_mask |= UINT32_C(1) << context_id;
            planned.context_models[context_id] = contexts[context_id].model;
        }
    }
    std::uint64_t payload_bits = bypass_bits_;
    for (std::size_t context_id = 0;
         context_id < context_frequencies_.size(); ++context_id) {
        const bool overridden =
            (planned.override_mask & (UINT32_C(1) << context_id)) != 0;
        const auto& model = overridden
            ? planned.context_models[context_id]
            : planned.field_models[
                  contextual_blocked_huffman_field_for_context(
                      static_cast<std::uint16_t>(context_id))];
        const auto alphabet = (*layout_.alphabets)[context_id];
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            std::uint64_t contribution{};
            if (!core::checked_multiply(
                    context_frequencies_[context_id][symbol],
                    static_cast<std::uint64_t>(model_length(model, symbol)),
                    contribution)
                || !core::checked_add(
                    payload_bits, contribution, payload_bits)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
        }
    }
    const auto payload_size = (payload_bits + 7U) / 8U;
    if (payload_size > UINT32_MAX) {
        result.error = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    planned.payload_size = static_cast<std::uint32_t>(payload_size);
    planned.final_valid_bits = payload_size == 0
        ? 0
        : static_cast<std::uint8_t>(
              payload_bits % 8U == 0 ? 8U : payload_bits % 8U);
    std::size_t descriptor_size{};
    const auto format_error = validate_contextual_blocked_huffman_descriptor(
        planned, planned.decision_count, planned.payload_size, limits,
        descriptor_size, layout_.context_variant);
    if (format_error == ContextualBlockedHuffmanFormatError::limit_exceeded) {
        result.error = ContextualBlockedHuffmanEncodeError::limit_exceeded;
        return result;
    }
    if (format_error
        == ContextualBlockedHuffmanFormatError::arithmetic_overflow) {
        result.error = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    if (format_error == ContextualBlockedHuffmanFormatError::invalid_field_mask
        || format_error
            == ContextualBlockedHuffmanFormatError::invalid_override_mask) {
        result.error = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return result;
    }
    if (format_error != ContextualBlockedHuffmanFormatError::none) {
        result.error = ContextualBlockedHuffmanEncodeError::internal_error;
        return result;
    }
    result.decision_count = planned.decision_count;
    result.descriptor_size = descriptor_size;
    result.payload_size = static_cast<std::size_t>(payload_size);
    descriptor = planned;
    return result;
}

ContextualBlockedHuffmanWriter::ContextualBlockedHuffmanWriter(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::span<std::byte> payload_output,
    const context::internal::LzssFieldContextVariant variant) noexcept
    : descriptor_(&descriptor), output_(payload_output) {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    layout_ = selected.layout;
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        error_ = ContextualBlockedHuffmanEncodeError::
            unsupported_context_variant;
        return;
    }
    if (output_.size() < descriptor.payload_size) {
        error_ = ContextualBlockedHuffmanEncodeError::payload_output_too_small;
        return;
    }
    output_ = output_.first(descriptor.payload_size);
    std::ranges::fill(output_, std::byte{0});
}

ContextualBlockedHuffmanEncodeError
ContextualBlockedHuffmanWriter::encode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet_size,
    const std::uint32_t value) noexcept {
    if (error_ != ContextualBlockedHuffmanEncodeError::none) return error_;
    if (!valid_symbol({
            ModeledOperationKind::symbol, context_id, alphabet_size, value,
            0}, layout_)) {
        error_ = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return error_;
    }
    const bool overridden =
        (descriptor_->override_mask & (UINT32_C(1) << context_id)) != 0;
    const auto table_index = overridden
        ? 4U + context_id
        : contextual_blocked_huffman_field_for_context(context_id);
    const auto& model = overridden
        ? descriptor_->context_models[context_id]
        : descriptor_->field_models[table_index];
    if (!model.active
        || (model.single_symbol
                != contextual_blocked_huffman_no_single_symbol
            && model.single_symbol != value)) {
        error_ = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return error_;
    }
    if (model.single_symbol == contextual_blocked_huffman_no_single_symbol) {
        if (!built_[table_index]) {
            if (build_canonical_table(model.lengths, tables_[table_index])
                != HuffmanTableError::none) {
                error_ = ContextualBlockedHuffmanEncodeError::internal_error;
                return error_;
            }
            built_[table_index] = true;
        }
        const auto& code = tables_[table_index][value];
        for (std::uint8_t bit = 0; bit < code.length; ++bit) {
            if (bit_offset_ >= UINT64_C(8) * output_.size()) {
                error_ =
                    ContextualBlockedHuffmanEncodeError::internal_error;
                return error_;
            }
            if (((code.lsb_first >> bit) & 1U) != 0) {
                output_[static_cast<std::size_t>(bit_offset_ / 8U)]
                    |= static_cast<std::byte>(1U << (bit_offset_ % 8U));
            }
            ++bit_offset_;
        }
    }
    ++decision_count_;
    ++operation_count_;
    return error_;
}

ContextualBlockedHuffmanEncodeError
ContextualBlockedHuffmanWriter::encode_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (error_ != ContextualBlockedHuffmanEncodeError::none) return error_;
    if (!valid_bypass(
            {ModeledOperationKind::bypass_bits, 0, 0, value, bit_count},
            layout_)) {
        error_ = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return error_;
    }
    for (std::uint8_t bit = 0; bit < bit_count; ++bit) {
        if (bit_offset_ >= UINT64_C(8) * output_.size()) {
            error_ = ContextualBlockedHuffmanEncodeError::internal_error;
            return error_;
        }
        if (((value >> bit) & 1U) != 0) {
            output_[static_cast<std::size_t>(bit_offset_ / 8U)]
                |= static_cast<std::byte>(1U << (bit_offset_ % 8U));
        }
        ++bit_offset_;
    }
    decision_count_ += bit_count;
    ++operation_count_;
    return error_;
}

ContextualBlockedHuffmanEncodeError
ContextualBlockedHuffmanWriter::finish(
    const std::size_t expected_operation_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (error_ != ContextualBlockedHuffmanEncodeError::none) return error_;
    const auto expected_bits = descriptor_->payload_size == 0
        ? UINT64_C(0)
        : UINT64_C(8) * (descriptor_->payload_size - 1U)
            + descriptor_->final_valid_bits;
    if (operation_count_ != expected_operation_count
        || decision_count_ != expected_decision_count
        || bit_offset_ != expected_bits) {
        error_ = ContextualBlockedHuffmanEncodeError::internal_error;
    }
    return error_;
}

ContextualBlockedHuffmanEncodeResult
plan_contextual_blocked_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualBlockedHuffmanEncodeResult result{};
    result.operation_count = operations.size();
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = ContextualBlockedHuffmanEncodeError::
            unsupported_context_variant;
        return result;
    }
    const auto& layout = selected.layout;
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = ContextualBlockedHuffmanEncodeError::limit_exceeded;
        return result;
    }
    std::array<HuffmanFrequencies,
               context::internal::lzss_field_context_count>
        context_frequencies{};
    std::array<HuffmanFrequencies, 4> field_frequencies{};
    std::uint64_t decisions{};
    for (std::size_t index = 0; index < operations.size(); ++index) {
        result.operation_index = index;
        const auto& operation = operations[index];
        if (valid_bypass(operation, layout)) {
            if (!core::checked_add(
                    decisions, static_cast<std::uint64_t>(operation.bit_count),
                    decisions)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
            continue;
        }
        if (!valid_symbol(operation, layout)) {
            result.error =
                ContextualBlockedHuffmanEncodeError::invalid_operation;
            return result;
        }
        auto& context_frequency =
            context_frequencies[operation.context_id][operation.value];
        const auto field = contextual_blocked_huffman_field_for_context(
            operation.context_id);
        auto& field_frequency = field_frequencies[field][operation.value];
        if (context_frequency == std::numeric_limits<std::uint64_t>::max()
            || field_frequency == std::numeric_limits<std::uint64_t>::max()) {
            result.error =
                ContextualBlockedHuffmanEncodeError::frequency_overflow;
            return result;
        }
        ++context_frequency;
        ++field_frequency;
        if (!core::checked_add(decisions, UINT64_C(1), decisions)) {
            result.error =
                ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
            return result;
        }
    }
    result.operation_index = operations.size();
    if (decisions == 0 || decisions > UINT32_MAX) {
        result.error = operations.empty()
            ? ContextualBlockedHuffmanEncodeError::invalid_operation
            : ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }

    ContextualBlockedHuffmanDescriptor planned{};
    planned.decision_count = static_cast<std::uint32_t>(decisions);
    const auto selected_field_alphabets = field_alphabets(layout);
    std::array<BuiltModel, 4> fields{};
    for (std::size_t field = 0; field < fields.size(); ++field) {
        const auto error = build_model(
            field_frequencies[field], selected_field_alphabets[field],
            fields[field]);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            result.error = error;
            return result;
        }
        if (!fields[field].model.active) continue;
        planned.field_active_mask |= static_cast<std::uint8_t>(1U << field);
        planned.field_models[field] = fields[field].model;
    }

    std::array<BuiltModel, context::internal::lzss_field_context_count>
        contexts{};
    for (std::size_t context_id = 0; context_id < contexts.size(); ++context_id) {
        const auto error = build_model(
            context_frequencies[context_id],
            (*layout.alphabets)[context_id],
            contexts[context_id]);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            result.error = error;
            return result;
        }
        if (!contexts[context_id].model.active) continue;
        const auto field = contextual_blocked_huffman_field_for_context(
            static_cast<std::uint16_t>(context_id));
        std::uint64_t base_bits{};
        for (std::uint16_t symbol = 0;
             symbol < (*layout.alphabets)[context_id];
             ++symbol) {
            std::uint64_t contribution{};
            if (!core::checked_multiply(
                    context_frequencies[context_id][symbol],
                    static_cast<std::uint64_t>(
                        model_length(fields[field].model, symbol)),
                    contribution)
                || !core::checked_add(base_bits, contribution, base_bits)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
        }
        if (base_bits <= contexts[context_id].symbol_bits) continue;
        const auto savings = base_bits - contexts[context_id].symbol_bits;
        std::uint64_t record_bits{};
        if (!core::checked_multiply(
                static_cast<std::uint64_t>(contexts[context_id].record_size),
                UINT64_C(8), record_bits)) {
            result.error =
                ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
            return result;
        }
        if (savings <= record_bits) continue;
        planned.override_mask |= UINT32_C(1) << context_id;
        planned.context_models[context_id] = contexts[context_id].model;
    }

    std::uint64_t payload_bits{};
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (!core::checked_add(
                    payload_bits,
                    static_cast<std::uint64_t>(operation.bit_count),
                    payload_bits)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
            continue;
        }
        const auto overridden =
            (planned.override_mask & (UINT32_C(1) << operation.context_id))
            != 0;
        const auto& model = overridden
            ? planned.context_models[operation.context_id]
            : planned.field_models[
                  contextual_blocked_huffman_field_for_context(
                      operation.context_id)];
        if (!core::checked_add(
                payload_bits,
                static_cast<std::uint64_t>(
                    model_length(
                        model, static_cast<std::uint16_t>(operation.value))),
                payload_bits)) {
            result.error =
                ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
            return result;
        }
    }
    const auto payload_size = (payload_bits + 7U) / 8U;
    if (payload_size > UINT32_MAX) {
        result.error = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    planned.payload_size = static_cast<std::uint32_t>(payload_size);
    planned.final_valid_bits = payload_size == 0
        ? 0
        : static_cast<std::uint8_t>(
              payload_bits % 8U == 0 ? 8U : payload_bits % 8U);
    std::size_t descriptor_size{};
    const auto format_error = validate_contextual_blocked_huffman_descriptor(
        planned, planned.decision_count, planned.payload_size, limits,
        descriptor_size, variant);
    if (format_error == ContextualBlockedHuffmanFormatError::limit_exceeded) {
        result.error = ContextualBlockedHuffmanEncodeError::limit_exceeded;
        return result;
    }
    if (format_error
        == ContextualBlockedHuffmanFormatError::arithmetic_overflow) {
        result.error = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return result;
    }
    if (format_error == ContextualBlockedHuffmanFormatError::invalid_field_mask
        || format_error
            == ContextualBlockedHuffmanFormatError::invalid_override_mask) {
        result.error = ContextualBlockedHuffmanEncodeError::invalid_operation;
        return result;
    }
    if (format_error != ContextualBlockedHuffmanFormatError::none) {
        result.error = ContextualBlockedHuffmanEncodeError::internal_error;
        return result;
    }
    result.decision_count = planned.decision_count;
    result.descriptor_size = descriptor_size;
    result.payload_size = static_cast<std::size_t>(payload_size);
    descriptor = planned;
    return result;
}

ContextualBlockedHuffmanEncodeResult
encode_contextual_blocked_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    ContextualBlockedHuffmanDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualBlockedHuffmanEncodeResult initial{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(ModeledOperation), operation_bytes)) {
        initial.error = ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
        return initial;
    }
    bool overlap_overflow{};
    if (ranges_overlap(
            operations.data(), operation_bytes, payload_output.data(),
            payload_output.size(), overlap_overflow)
        || overlap_overflow) {
        initial.error = overlap_overflow
            ? ContextualBlockedHuffmanEncodeError::arithmetic_overflow
            : ContextualBlockedHuffmanEncodeError::overlapping_buffers;
        return initial;
    }
    ContextualBlockedHuffmanDescriptor planned{};
    auto result = plan_contextual_blocked_huffman_operations(
        operations, limits, planned, variant);
    if (result.error != ContextualBlockedHuffmanEncodeError::none) return result;
    if (payload_output.size() < result.payload_size) {
        result.error =
            ContextualBlockedHuffmanEncodeError::payload_output_too_small;
        return result;
    }
    ContextualBlockedHuffmanWriter writer(planned, payload_output, variant);
    for (const auto& operation : operations) {
        const auto error = operation.kind == ModeledOperationKind::symbol
            ? writer.encode_symbol(
                  operation.context_id, operation.alphabet_size,
                  operation.value)
            : writer.encode_bypass(operation.bit_count, operation.value);
        if (error != ContextualBlockedHuffmanEncodeError::none) {
            result.error = error;
            return result;
        }
    }
    const auto finish_error = writer.finish(
        operations.size(), result.decision_count);
    if (finish_error != ContextualBlockedHuffmanEncodeError::none) {
        result.error = finish_error;
        return result;
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal

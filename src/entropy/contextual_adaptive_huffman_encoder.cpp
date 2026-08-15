#include "entropy/contextual_adaptive_huffman_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

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

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult fail(
    ContextualAdaptiveHuffmanEncodeResult result,
    const ContextualAdaptiveHuffmanEncodeError error) noexcept {
    result.error = error;
    return result;
}

struct Extents {
    std::size_t operation_bytes{};
    std::size_t node_entries{};
    std::size_t symbol_entries{};
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
};

[[nodiscard]] bool calculate_extents(
    const std::span<const ModeledOperation> operations,
    const context::internal::LzssFieldContextLayout& layout,
    Extents& extents) noexcept {
    extents.symbol_entries = layout.frequency_entries;
    return core::checked_multiply(
               operations.size(), sizeof(ModeledOperation),
               extents.operation_bytes)
        && core::checked_multiply(
            layout.frequency_entries, std::size_t{2}, extents.node_entries)
        && core::checked_add(
            extents.node_entries,
            static_cast<std::size_t>(context::internal::lzss_field_context_count),
            extents.node_entries)
        && core::checked_multiply(
            extents.node_entries,
            sizeof(AdaptiveHuffmanNode), extents.node_bytes)
        && core::checked_multiply(
            extents.symbol_entries,
            sizeof(std::uint16_t), extents.symbol_bytes);
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError validate_regions(
    const std::span<const ModeledOperation> operations,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<std::byte> payload,
    const Extents& extents) noexcept {
    const std::array overlaps{
        ranges_overlap(
            operations.data(), extents.operation_bytes,
            nodes.data(), extents.node_bytes),
        ranges_overlap(
            operations.data(), extents.operation_bytes,
            symbols.data(), extents.symbol_bytes),
        ranges_overlap(
            nodes.data(), extents.node_bytes,
            symbols.data(), extents.symbol_bytes),
        ranges_overlap(
            operations.data(), extents.operation_bytes,
            payload.data(), payload.size()),
        ranges_overlap(
            nodes.data(), extents.node_bytes,
            payload.data(), payload.size()),
        ranges_overlap(
            symbols.data(), extents.symbol_bytes,
            payload.data(), payload.size()),
    };
    if (std::ranges::find(
            overlaps, OverlapCheck::arithmetic_overflow) != overlaps.end()) {
        return ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()) {
        return ContextualAdaptiveHuffmanEncodeError::overlapping_buffers;
    }
    return ContextualAdaptiveHuffmanEncodeError::none;
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError add_bits(
    const std::size_t increment,
    ContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    return core::checked_add(result.payload_bits, increment, result.payload_bits)
        ? ContextualAdaptiveHuffmanEncodeError::none
        : ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
}

void write_bit(
    const std::uint8_t bit, const std::size_t offset,
    const std::span<std::byte> output) noexcept {
    if (bit != 0) {
        output[offset / 8U] |=
            static_cast<std::byte>(1U << (offset % 8U));
    }
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError emit_bits(
    const std::span<const std::uint8_t> bits,
    const std::span<std::byte> output,
    ContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    const auto start = result.payload_bits;
    const auto error = add_bits(bits.size(), result);
    if (error != ContextualAdaptiveHuffmanEncodeError::none) return error;
    if (!output.empty()) {
        std::size_t capacity_bits{};
        if (!core::checked_multiply(
                output.size(), std::size_t{8}, capacity_bits)
            || result.payload_bits > capacity_bits) {
            return ContextualAdaptiveHuffmanEncodeError::internal_error;
        }
        for (std::size_t index = 0; index < bits.size(); ++index) {
            write_bit(bits[index], start + index, output);
        }
    }
    return ContextualAdaptiveHuffmanEncodeError::none;
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError emit_value(
    const std::uint32_t value, const std::uint8_t bit_count,
    const std::span<std::byte> output,
    ContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    const auto start = result.payload_bits;
    const auto error = add_bits(bit_count, result);
    if (error != ContextualAdaptiveHuffmanEncodeError::none) return error;
    if (!output.empty()) {
        std::size_t capacity_bits{};
        if (!core::checked_multiply(
                output.size(), std::size_t{8}, capacity_bits)
            || result.payload_bits > capacity_bits) {
            return ContextualAdaptiveHuffmanEncodeError::internal_error;
        }
        for (std::uint8_t bit = 0; bit < bit_count; ++bit) {
            write_bit(
                static_cast<std::uint8_t>((value >> bit) & 1U),
                start + bit, output);
        }
    }
    return ContextualAdaptiveHuffmanEncodeError::none;
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError emit_symbol(
    ContextualAdaptiveHuffmanModelBank& models,
    const context::internal::LzssFieldContextLayout& layout,
    const ModeledOperation& operation,
    const std::span<std::byte> output,
    ContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    if (operation.context_id >= context::internal::lzss_field_context_count) {
        return ContextualAdaptiveHuffmanEncodeError::invalid_context;
    }
    if (operation.alphabet_size
        != (*layout.alphabets)[operation.context_id]) {
        return ContextualAdaptiveHuffmanEncodeError::invalid_alphabet;
    }
    if (operation.value >= operation.alphabet_size) {
        return ContextualAdaptiveHuffmanEncodeError::invalid_symbol;
    }
    if (operation.bit_count != 0) {
        return ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field;
    }
    auto* tree = models.tree(operation.context_id);
    if (tree == nullptr) return ContextualAdaptiveHuffmanEncodeError::tree_error;
    const auto symbol = static_cast<std::uint16_t>(operation.value);
    const bool is_new = !tree->contains(symbol);
    std::array<std::uint8_t, contextual_adaptive_huffman_max_alphabet> path{};
    std::size_t path_size{};
    const auto path_error = is_new
        ? tree->path_for_nyt(path, path_size)
        : tree->path_for_symbol(symbol, path, path_size);
    if (path_error != ContextualAdaptiveHuffmanTreeError::none) {
        return ContextualAdaptiveHuffmanEncodeError::tree_error;
    }
    auto error = emit_bits(
        std::span<const std::uint8_t>{path}.first(path_size), output, result);
    if (error != ContextualAdaptiveHuffmanEncodeError::none) return error;
    if (is_new) {
        const auto raw_width = static_cast<std::uint8_t>(std::bit_width(
            static_cast<std::uint16_t>(operation.alphabet_size - 1U)));
        error = emit_value(symbol, raw_width, output, result);
        if (error != ContextualAdaptiveHuffmanEncodeError::none) return error;
    }
    const auto tree_error = is_new
        ? tree->observe_new(symbol)
        : tree->observe_existing(symbol);
    return tree_error == ContextualAdaptiveHuffmanTreeError::none
        ? ContextualAdaptiveHuffmanEncodeError::none
        : ContextualAdaptiveHuffmanEncodeError::tree_error;
}

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult run(
    const std::span<const ModeledOperation> operations,
    const std::span<AdaptiveHuffmanNode> nodes,
    const std::span<std::uint16_t> symbols,
    const std::span<std::byte> output,
    const context::internal::LzssFieldContextLayout& layout,
    const Extents& extents) noexcept {
    ContextualAdaptiveHuffmanEncodeResult result{};
    ContextualAdaptiveHuffmanModelBank models;
    if (models.initialize(
            layout, nodes.first(extents.node_entries),
            symbols.first(extents.symbol_entries))
            != ContextualAdaptiveHuffmanModelError::none
        || !models.validate()) {
        return fail(result, ContextualAdaptiveHuffmanEncodeError::tree_error);
    }
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto kind = static_cast<std::uint8_t>(operation.kind);
        ContextualAdaptiveHuffmanEncodeError error{};
        if (kind > static_cast<std::uint8_t>(ModeledOperationKind::bypass_bits)) {
            error = ContextualAdaptiveHuffmanEncodeError::invalid_operation_kind;
        } else if (operation.kind == ModeledOperationKind::symbol) {
            error = emit_symbol(models, layout, operation, output, result);
            if (error == ContextualAdaptiveHuffmanEncodeError::none) {
                if (result.decision_count == UINT32_MAX) {
                    error = ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
                } else {
                    ++result.decision_count;
                }
            }
        } else if (operation.context_id != 0
                   || operation.alphabet_size != 0) {
            error = ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field;
        } else if (operation.bit_count == 0
                   || operation.bit_count > layout.maximum_bypass_bits) {
            error = ContextualAdaptiveHuffmanEncodeError::invalid_bypass_width;
        } else if ((operation.value >> operation.bit_count) != 0) {
            error = ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field;
        } else {
            std::uint32_t updated{};
            if (!core::checked_add(
                    result.decision_count,
                    static_cast<std::uint32_t>(operation.bit_count), updated)) {
                error = ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow;
            } else {
                error = emit_value(
                    operation.value, operation.bit_count, output, result);
                if (error == ContextualAdaptiveHuffmanEncodeError::none) {
                    result.decision_count = updated;
                }
            }
        }
        if (error != ContextualAdaptiveHuffmanEncodeError::none) {
            return fail(result, error);
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    if (!models.validate()) {
        return fail(result, ContextualAdaptiveHuffmanEncodeError::tree_error);
    }
    return result;
}

} // namespace

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::fail(
    const ContextualAdaptiveHuffmanEncodeError error) noexcept {
    result_.error = error;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::begin_common(
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    const std::span<std::byte> payload_output,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    models_ = {};
    layout_ = {};
    limits_ = limits;
    output_ = {};
    expected_ = {};
    result_ = {};
    started_ = false;
    writing_ = false;
    finished_ = false;
    if (core::validate_limits(limits_) != core::LimitError::none) {
        return fail(ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::invalid_context_variant);
    }
    Extents extents{};
    if (!calculate_extents({}, selected.layout, extents)) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (node_workspace.size() < extents.node_entries) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::node_workspace_too_small);
    }
    if (symbol_workspace.size() < extents.symbol_entries) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::symbol_workspace_too_small);
    }
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    std::size_t aggregate{};
    node_bytes = extents.node_bytes;
    symbol_bytes = extents.symbol_bytes;
    if (!core::checked_add(node_bytes, symbol_bytes, aggregate)
        || !core::checked_add(aggregate, payload_output.size(), aggregate)) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    const std::array overlaps{
        ranges_overlap(
            node_workspace.data(), node_bytes,
            symbol_workspace.data(), symbol_bytes),
        ranges_overlap(
            node_workspace.data(), node_bytes,
            payload_output.data(), payload_output.size()),
        ranges_overlap(
            symbol_workspace.data(), symbol_bytes,
            payload_output.data(), payload_output.size()),
    };
    if (std::ranges::find(
            overlaps, OverlapCheck::arithmetic_overflow) != overlaps.end()) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()) {
        return fail(ContextualAdaptiveHuffmanEncodeError::overlapping_buffers);
    }
    std::size_t model_entries{};
    if (!core::checked_add(
            extents.node_entries, extents.symbol_entries, model_entries)) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (model_entries > limits_.max_entropy_table_entries
        || aggregate > limits_.max_internal_buffered_bytes) {
        return fail(ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (models_.initialize(
            selected.layout, node_workspace.first(extents.node_entries),
            symbol_workspace.first(extents.symbol_entries))
            != ContextualAdaptiveHuffmanModelError::none
        || !models_.validate()) {
        return fail(ContextualAdaptiveHuffmanEncodeError::tree_error);
    }
    layout_ = selected.layout;
    output_ = payload_output;
    started_ = true;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::begin_plan(
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    return begin_common(limits, node_workspace, symbol_workspace, {}, variant);
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::begin_write(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    const std::span<std::byte> payload_output,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    if (payload_output.size() < descriptor.payload_size) {
        models_ = {};
        layout_ = {};
        limits_ = limits;
        output_ = {};
        expected_ = {};
        result_ = {};
        started_ = false;
        writing_ = false;
        finished_ = false;
        return fail(
            ContextualAdaptiveHuffmanEncodeError::payload_output_too_small);
    }
    const auto payload = payload_output.first(descriptor.payload_size);
    auto result = begin_common(
        limits, node_workspace, symbol_workspace, payload, variant);
    if (result.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result;
    }
    if (validate_contextual_adaptive_huffman_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            limits_)
        != ContextualAdaptiveHuffmanFormatError::none) {
        return fail(ContextualAdaptiveHuffmanEncodeError::invalid_descriptor);
    }
    expected_ = descriptor;
    writing_ = true;
    std::ranges::fill(output_, std::byte{});
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::encode_symbol(
    const std::uint16_t context_id, const std::uint16_t alphabet_size,
    const std::uint32_t value) noexcept {
    if (!started_) {
        return result_.error == ContextualAdaptiveHuffmanEncodeError::none
            ? fail(ContextualAdaptiveHuffmanEncodeError::not_started)
            : result_;
    }
    if (result_.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result_;
    }
    if (finished_) {
        return fail(ContextualAdaptiveHuffmanEncodeError::already_finished);
    }
    const ModeledOperation operation{
        ModeledOperationKind::symbol, context_id, alphabet_size, value, 0};
    if (result_.decision_count == UINT32_MAX) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    const auto error = emit_symbol(models_, layout_, operation, output_, result_);
    if (error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail(error);
    }
    ++result_.decision_count;
    ++result_.operation_count;
    result_.operation_index = result_.operation_count;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::encode_bypass(
    const std::uint8_t bit_count, const std::uint32_t value) noexcept {
    if (!started_) {
        return result_.error == ContextualAdaptiveHuffmanEncodeError::none
            ? fail(ContextualAdaptiveHuffmanEncodeError::not_started)
            : result_;
    }
    if (result_.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result_;
    }
    if (finished_) {
        return fail(ContextualAdaptiveHuffmanEncodeError::already_finished);
    }
    if (bit_count == 0 || bit_count > layout_.maximum_bypass_bits) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::invalid_bypass_width);
    }
    if ((value >> bit_count) != 0) {
        return fail(
            ContextualAdaptiveHuffmanEncodeError::nonzero_unused_field);
    }
    std::uint32_t updated{};
    if (!core::checked_add(
            result_.decision_count, static_cast<std::uint32_t>(bit_count),
            updated)) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    const auto error = emit_value(value, bit_count, output_, result_);
    if (error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail(error);
    }
    result_.decision_count = updated;
    ++result_.operation_count;
    result_.operation_index = result_.operation_count;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::finish_plan(
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    if (!started_) {
        return result_.error == ContextualAdaptiveHuffmanEncodeError::none
            ? fail(ContextualAdaptiveHuffmanEncodeError::not_started)
            : result_;
    }
    if (result_.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result_;
    }
    if (finished_ || writing_) {
        return fail(ContextualAdaptiveHuffmanEncodeError::already_finished);
    }
    if (result_.operation_count == 0) {
        return fail(ContextualAdaptiveHuffmanEncodeError::empty_operations);
    }
    if (!models_.validate()) {
        return fail(ContextualAdaptiveHuffmanEncodeError::tree_error);
    }
    if (result_.decision_count
        > contextual_adaptive_huffman_max_decision_count) {
        return fail(ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (result_.payload_bits > std::numeric_limits<std::size_t>::max() - 7U) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    result_.payload_size = (result_.payload_bits + 7U) / 8U;
    if (result_.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    std::size_t aggregate{};
    std::size_t node_entries{};
    if (!core::checked_multiply(
            layout_.frequency_entries, std::size_t{2}, node_entries)
        || !core::checked_add(
            node_entries,
            static_cast<std::size_t>(context::internal::lzss_field_context_count),
            node_entries)
        || !core::checked_multiply(
            node_entries, sizeof(AdaptiveHuffmanNode), node_bytes)
        || !core::checked_multiply(
            layout_.frequency_entries,
            sizeof(std::uint16_t), symbol_bytes)
        || !core::checked_add(node_bytes, symbol_bytes, aggregate)
        || !core::checked_add(aggregate, result_.payload_size, aggregate)) {
        return fail(ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (result_.payload_size > limits_.max_compressed_payload_size
        || aggregate > limits_.max_internal_buffered_bytes) {
        return fail(ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    ContextualAdaptiveHuffmanDescriptor planned{};
    planned.decision_count = result_.decision_count;
    planned.payload_size = static_cast<std::uint32_t>(result_.payload_size);
    planned.final_valid_bits = static_cast<std::uint8_t>(
        result_.payload_bits % 8U == 0 ? 8U : result_.payload_bits % 8U);
    const auto format_error = validate_contextual_adaptive_huffman_descriptor(
        planned, planned.decision_count, planned.payload_size, limits_);
    if (format_error == ContextualAdaptiveHuffmanFormatError::limit_exceeded) {
        return fail(ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (format_error != ContextualAdaptiveHuffmanFormatError::none) {
        return fail(ContextualAdaptiveHuffmanEncodeError::internal_error);
    }
    descriptor = planned;
    finished_ = true;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
ContextualAdaptiveHuffmanForwardEncoder::finish_write(
    const std::size_t expected_operation_count,
    const std::uint32_t expected_decision_count,
    const std::size_t expected_payload_bits) noexcept {
    if (!started_) {
        return result_.error == ContextualAdaptiveHuffmanEncodeError::none
            ? fail(ContextualAdaptiveHuffmanEncodeError::not_started)
            : result_;
    }
    if (result_.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result_;
    }
    if (finished_ || !writing_) {
        return fail(ContextualAdaptiveHuffmanEncodeError::already_finished);
    }
    const auto payload_size = (result_.payload_bits + 7U) / 8U;
    if (result_.operation_count != expected_operation_count
        || result_.decision_count != expected_decision_count
        || result_.decision_count != expected_.decision_count
        || result_.payload_bits != expected_payload_bits
        || payload_size != expected_.payload_size
        || !models_.validate()) {
        return fail(ContextualAdaptiveHuffmanEncodeError::internal_error);
    }
    const auto final_bits = static_cast<std::uint8_t>(
        result_.payload_bits % 8U == 0 ? 8U : result_.payload_bits % 8U);
    if (final_bits != expected_.final_valid_bits) {
        return fail(ContextualAdaptiveHuffmanEncodeError::internal_error);
    }
    result_.payload_size = payload_size;
    finished_ = true;
    return result_;
}

ContextualAdaptiveHuffmanEncodeResult
plan_contextual_adaptive_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    ContextualAdaptiveHuffmanDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualAdaptiveHuffmanEncodeResult initial{};
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return fail(
            initial,
            ContextualAdaptiveHuffmanEncodeError::invalid_context_variant);
    }
    if (operations.empty()) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::empty_operations);
    }
    if (core::validate_limits(limits) != core::LimitError::none) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    Extents extents{};
    if (!calculate_extents(operations, selected.layout, extents)) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (node_workspace.size() < extents.node_entries) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::
                        node_workspace_too_small);
    }
    if (symbol_workspace.size() < extents.symbol_entries) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::
                        symbol_workspace_too_small);
    }
    const auto region_error = validate_regions(
        operations, node_workspace, symbol_workspace, {}, extents);
    if (region_error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail(initial, region_error);
    }
    std::size_t aggregate{};
    if (!core::checked_add(extents.operation_bytes, extents.node_bytes, aggregate)
        || !core::checked_add(aggregate, extents.symbol_bytes, aggregate)) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    std::size_t model_entries{};
    if (!core::checked_add(
            extents.node_entries, extents.symbol_entries, model_entries)) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (model_entries > limits.max_entropy_table_entries
        || aggregate > limits.max_internal_buffered_bytes) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }

    auto result = run(
        operations, node_workspace, symbol_workspace, {}, selected.layout,
        extents);
    if (result.error != ContextualAdaptiveHuffmanEncodeError::none) {
        return result;
    }
    if (result.decision_count
        > contextual_adaptive_huffman_max_decision_count) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (result.payload_bits > std::numeric_limits<std::size_t>::max() - 7U) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    result.payload_size = (result.payload_bits + 7U) / 8U;
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (result.payload_size > limits.max_compressed_payload_size) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    ContextualAdaptiveHuffmanDescriptor planned{};
    planned.decision_count = result.decision_count;
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    planned.final_valid_bits = static_cast<std::uint8_t>(
        result.payload_bits % 8U == 0 ? 8U : result.payload_bits % 8U);
    const auto format_error = validate_contextual_adaptive_huffman_descriptor(
        planned, planned.decision_count, planned.payload_size, limits);
    if (format_error == ContextualAdaptiveHuffmanFormatError::limit_exceeded) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (format_error != ContextualAdaptiveHuffmanFormatError::none) {
        return fail(result,
                    ContextualAdaptiveHuffmanEncodeError::internal_error);
    }
    descriptor = planned;
    return result;
}

ContextualAdaptiveHuffmanEncodeResult
encode_contextual_adaptive_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    const std::span<std::byte> payload_output,
    ContextualAdaptiveHuffmanDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return fail(
            {}, ContextualAdaptiveHuffmanEncodeError::invalid_context_variant);
    }
    ContextualAdaptiveHuffmanDescriptor planned{};
    auto plan = plan_contextual_adaptive_huffman_operations(
        operations, limits, node_workspace, symbol_workspace, planned,
        variant);
    if (plan.error != ContextualAdaptiveHuffmanEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return fail(
            plan,
            ContextualAdaptiveHuffmanEncodeError::payload_output_too_small);
    }
    const auto payload = payload_output.first(plan.payload_size);
    Extents extents{};
    if (!calculate_extents(operations, selected.layout, extents)) {
        return fail(plan,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    const auto region_error = validate_regions(
        operations, node_workspace, symbol_workspace, payload, extents);
    if (region_error != ContextualAdaptiveHuffmanEncodeError::none) {
        return fail(plan, region_error);
    }
    std::size_t aggregate{};
    if (!core::checked_add(extents.operation_bytes, extents.node_bytes, aggregate)
        || !core::checked_add(aggregate, extents.symbol_bytes, aggregate)
        || !core::checked_add(aggregate, payload.size(), aggregate)) {
        return fail(plan,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
    }
    if (aggregate > limits.max_internal_buffered_bytes) {
        return fail(plan, ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    std::ranges::fill(payload, std::byte{});
    const auto encoded = run(
        operations, node_workspace, symbol_workspace, payload,
        selected.layout, extents);
    if (encoded.error != ContextualAdaptiveHuffmanEncodeError::none
        || encoded.operation_count != plan.operation_count
        || encoded.decision_count != plan.decision_count
        || encoded.payload_bits != plan.payload_bits) {
        return fail(plan, ContextualAdaptiveHuffmanEncodeError::internal_error);
    }
    descriptor = planned;
    auto result = encoded;
    result.payload_size = plan.payload_size;
    return result;
}

} // namespace marc::entropy::internal

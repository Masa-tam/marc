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
    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
};

[[nodiscard]] bool calculate_extents(
    const std::span<const ModeledOperation> operations,
    Extents& extents) noexcept {
    return core::checked_multiply(
               operations.size(), sizeof(ModeledOperation),
               extents.operation_bytes)
        && core::checked_multiply(
            contextual_adaptive_huffman_node_entries,
            sizeof(AdaptiveHuffmanNode), extents.node_bytes)
        && core::checked_multiply(
            contextual_adaptive_huffman_symbol_entries,
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

[[nodiscard]] ContextualAdaptiveHuffmanEncodeError encode_symbol(
    ContextualAdaptiveHuffmanModelBank& models,
    const ModeledOperation& operation,
    const std::span<std::byte> output,
    ContextualAdaptiveHuffmanEncodeResult& result) noexcept {
    if (operation.context_id >= context::internal::lzss_field_context_count) {
        return ContextualAdaptiveHuffmanEncodeError::invalid_context;
    }
    if (operation.alphabet_size
        != context::internal::lzss_field_context_alphabets[
            operation.context_id]) {
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
    const std::span<std::byte> output) noexcept {
    ContextualAdaptiveHuffmanEncodeResult result{};
    ContextualAdaptiveHuffmanModelBank models;
    if (models.initialize(
            nodes.first(contextual_adaptive_huffman_node_entries),
            symbols.first(contextual_adaptive_huffman_symbol_entries))
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
            error = encode_symbol(models, operation, output, result);
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
        } else if (operation.bit_count == 0 || operation.bit_count > 16) {
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
plan_contextual_adaptive_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_workspace,
    const std::span<std::uint16_t> symbol_workspace,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    ContextualAdaptiveHuffmanEncodeResult initial{};
    if (operations.empty()) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::empty_operations);
    }
    if (core::validate_limits(limits) != core::LimitError::none) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }
    if (node_workspace.size() < contextual_adaptive_huffman_node_entries) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::
                        node_workspace_too_small);
    }
    if (symbol_workspace.size()
        < contextual_adaptive_huffman_symbol_entries) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::
                        symbol_workspace_too_small);
    }
    Extents extents{};
    if (!calculate_extents(operations, extents)) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::arithmetic_overflow);
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
    if (contextual_adaptive_huffman_node_entries
                + contextual_adaptive_huffman_symbol_entries
            > limits.max_entropy_table_entries
        || aggregate > limits.max_internal_buffered_bytes) {
        return fail(initial,
                    ContextualAdaptiveHuffmanEncodeError::limit_exceeded);
    }

    auto result = run(operations, node_workspace, symbol_workspace, {});
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
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    ContextualAdaptiveHuffmanDescriptor planned{};
    auto plan = plan_contextual_adaptive_huffman_operations(
        operations, limits, node_workspace, symbol_workspace, planned);
    if (plan.error != ContextualAdaptiveHuffmanEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return fail(
            plan,
            ContextualAdaptiveHuffmanEncodeError::payload_output_too_small);
    }
    const auto payload = payload_output.first(plan.payload_size);
    Extents extents{};
    if (!calculate_extents(operations, extents)) {
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
        operations, node_workspace, symbol_workspace, payload);
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

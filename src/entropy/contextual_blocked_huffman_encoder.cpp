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

constexpr std::array<std::uint16_t, 4> field_alphabets{2, 256, 8, 17};

struct BuiltModel {
    ContextualBlockedHuffmanModel model{};
    std::uint64_t symbol_bits{};
    std::size_t record_size{};
};

[[nodiscard]] bool valid_symbol(const ModeledOperation& operation) noexcept {
    return operation.kind == ModeledOperationKind::symbol
        && operation.bit_count == 0
        && operation.context_id < context::internal::lzss_field_context_count
        && operation.alphabet_size
            == context::internal::lzss_field_context_alphabets[
                operation.context_id]
        && operation.value < operation.alphabet_size;
}

[[nodiscard]] bool valid_bypass(const ModeledOperation& operation) noexcept {
    return operation.kind == ModeledOperationKind::bypass_bits
        && operation.context_id == 0 && operation.alphabet_size == 0
        && operation.bit_count != 0 && operation.bit_count <= 16
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

ContextualBlockedHuffmanEncodeResult
plan_contextual_blocked_huffman_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor) noexcept {
    ContextualBlockedHuffmanEncodeResult result{};
    result.operation_count = operations.size();
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
        if (valid_bypass(operation)) {
            if (!core::checked_add(
                    decisions, static_cast<std::uint64_t>(operation.bit_count),
                    decisions)) {
                result.error =
                    ContextualBlockedHuffmanEncodeError::arithmetic_overflow;
                return result;
            }
            continue;
        }
        if (!valid_symbol(operation)) {
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
    std::array<BuiltModel, 4> fields{};
    for (std::size_t field = 0; field < fields.size(); ++field) {
        const auto error = build_model(
            field_frequencies[field], field_alphabets[field], fields[field]);
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
            context::internal::lzss_field_context_alphabets[context_id],
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
             symbol < context::internal::lzss_field_context_alphabets[context_id];
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
        descriptor_size);
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
    ContextualBlockedHuffmanDescriptor& descriptor) noexcept {
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
        operations, limits, planned);
    if (result.error != ContextualBlockedHuffmanEncodeError::none) return result;
    if (payload_output.size() < result.payload_size) {
        result.error =
            ContextualBlockedHuffmanEncodeError::payload_output_too_small;
        return result;
    }
    auto output = payload_output.first(result.payload_size);
    std::ranges::fill(output, std::byte{0});
    std::array<CanonicalHuffmanTable,
               contextual_blocked_huffman_max_table_count>
        tables{};
    std::array<bool, contextual_blocked_huffman_max_table_count> built{};
    std::uint64_t bit_offset{};
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::bypass_bits) {
            for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
                if (((operation.value >> bit) & 1U) != 0) {
                    output[static_cast<std::size_t>(bit_offset / 8U)]
                        |= static_cast<std::byte>(1U << (bit_offset % 8U));
                }
                ++bit_offset;
            }
            continue;
        }
        const bool overridden =
            (planned.override_mask & (UINT32_C(1) << operation.context_id))
            != 0;
        const auto table_index = overridden
            ? 4U + operation.context_id
            : contextual_blocked_huffman_field_for_context(
                  operation.context_id);
        const auto& model = overridden
            ? planned.context_models[operation.context_id]
            : planned.field_models[table_index];
        if (model.single_symbol != contextual_blocked_huffman_no_single_symbol) {
            continue;
        }
        if (!built[table_index]) {
            if (build_canonical_table(model.lengths, tables[table_index])
                != HuffmanTableError::none) {
                result.error = ContextualBlockedHuffmanEncodeError::internal_error;
                return result;
            }
            built[table_index] = true;
        }
        const auto& code = tables[table_index][operation.value];
        for (std::uint8_t bit = 0; bit < code.length; ++bit) {
            if (((code.lsb_first >> bit) & 1U) != 0) {
                output[static_cast<std::size_t>(bit_offset / 8U)]
                    |= static_cast<std::byte>(1U << (bit_offset % 8U));
            }
            ++bit_offset;
        }
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal

#include "entropy/contextual_huffman_estimator.hpp"

#include "context/lzss_field_context_format.hpp"
#include "core/checked_math.hpp"
#include "entropy/canonical_huffman.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

inline constexpr std::uint64_t descriptor_header_bytes = 8;
inline constexpr std::uint64_t contextual_model_map_bytes =
    context::internal::lzss_field_context_count;
inline constexpr std::size_t field_table_count = 4;

struct Model {
    std::uint16_t alphabet{};
    HuffmanCodeLengths lengths{};
    std::uint64_t symbol_bits{};
    std::uint64_t descriptor_bytes{};
    bool active{};
};

[[nodiscard]] std::size_t field_table(
    const std::uint16_t context_id) noexcept {
    if (context_id <= 2) return 0;
    if (context_id <= 19) return 1;
    if (context_id <= 22) return 2;
    return 3;
}

[[nodiscard]] bool add_u64(const std::uint64_t left,
                           const std::uint64_t right,
                           std::uint64_t& output) noexcept {
    return core::checked_add(left, right, output);
}

[[nodiscard]] ContextualHuffmanEstimateError build_model(
    const HuffmanFrequencies& frequencies,
    const std::uint16_t alphabet,
    Model& model) noexcept {
    model = {};
    model.alphabet = alphabet;
    std::size_t nonzero{};
    std::uint16_t sole_symbol{};
    for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
        if (frequencies[symbol] == 0) continue;
        ++nonzero;
        sole_symbol = symbol;
    }
    if (nonzero == 0) return ContextualHuffmanEstimateError::none;
    model.active = true;
    if (nonzero == 1) {
        model.lengths[sole_symbol] = 1;
        model.descriptor_bytes = 4;
        return ContextualHuffmanEstimateError::none;
    }
    if (build_length_limited_code_lengths(frequencies, model.lengths)
        != HuffmanBuildError::none) {
        return ContextualHuffmanEstimateError::huffman_build_error;
    }
    for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
        std::uint64_t contribution{};
        if (!core::checked_multiply(
                frequencies[symbol],
                static_cast<std::uint64_t>(model.lengths[symbol]),
                contribution)
            || !add_u64(model.symbol_bits, contribution,
                        model.symbol_bits)) {
            return ContextualHuffmanEstimateError::arithmetic_overflow;
        }
    }
    const auto dense_bytes = static_cast<std::uint64_t>((alphabet + 1U) / 2U);
    const auto sparse_bytes = static_cast<std::uint64_t>(2U * nonzero);
    model.descriptor_bytes = 4U + std::min(dense_bytes, sparse_bytes);
    return ContextualHuffmanEstimateError::none;
}

[[nodiscard]] bool same_model(const Model& left,
                              const Model& right) noexcept {
    if (!left.active || !right.active || left.alphabet != right.alphabet) {
        return false;
    }
    return std::equal(left.lengths.begin(),
                      left.lengths.begin() + left.alphabet,
                      right.lengths.begin());
}

[[nodiscard]] ContextualHuffmanEstimateError calculate_symbol_bits(
    const HuffmanFrequencies& frequencies,
    const Model& model,
    std::uint64_t& bits) noexcept {
    bits = 0;
    for (std::uint16_t symbol = 0; symbol < model.alphabet; ++symbol) {
        std::uint64_t contribution{};
        if (!core::checked_multiply(
                frequencies[symbol],
                static_cast<std::uint64_t>(model.lengths[symbol]),
                contribution)
            || !add_u64(bits, contribution, bits)) {
            return ContextualHuffmanEstimateError::arithmetic_overflow;
        }
    }
    return ContextualHuffmanEstimateError::none;
}

[[nodiscard]] ContextualHuffmanEstimateError finish_estimate(
    ContextualHuffmanEstimate& estimate) noexcept {
    std::uint64_t payload_bits{};
    if (!add_u64(estimate.symbol_bits, estimate.bypass_bits, payload_bits)
        || payload_bits > std::numeric_limits<std::uint64_t>::max() - 7U) {
        return ContextualHuffmanEstimateError::arithmetic_overflow;
    }
    estimate.payload_bytes = (payload_bits + 7U) / 8U;
    if (!add_u64(estimate.descriptor_bytes, estimate.payload_bytes,
                 estimate.total_bytes)) {
        return ContextualHuffmanEstimateError::arithmetic_overflow;
    }
    return ContextualHuffmanEstimateError::none;
}

} // namespace

ContextualHuffmanEstimateResult estimate_contextual_huffman_cost(
    const std::span<const ModeledOperation> operations) noexcept {
    ContextualHuffmanEstimateResult result{};
    std::array<HuffmanFrequencies,
               context::internal::lzss_field_context_count>
        context_frequencies{};
    std::array<HuffmanFrequencies, field_table_count> field_frequencies{};
    std::uint64_t bypass_bits{};

    for (std::size_t index = 0; index < operations.size(); ++index) {
        result.operation_index = index;
        const auto& operation = operations[index];
        if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (operation.context_id != 0 || operation.alphabet_size != 0
                || operation.bit_count == 0 || operation.bit_count > 16
                || operation.value >= (UINT32_C(1) << operation.bit_count)) {
                result.error = ContextualHuffmanEstimateError::invalid_operation;
                return result;
            }
            if (!add_u64(bypass_bits, operation.bit_count, bypass_bits)) {
                result.error =
                    ContextualHuffmanEstimateError::arithmetic_overflow;
                return result;
            }
            continue;
        }
        if (operation.kind != ModeledOperationKind::symbol
            || operation.bit_count != 0
            || operation.context_id
                >= context::internal::lzss_field_context_count
            || operation.alphabet_size
                != context::internal::lzss_field_context_alphabets[
                    operation.context_id]
            || operation.value >= operation.alphabet_size) {
            result.error = ContextualHuffmanEstimateError::invalid_operation;
            return result;
        }
        auto& context_frequency =
            context_frequencies[operation.context_id][operation.value];
        auto& field_frequency =
            field_frequencies[field_table(operation.context_id)]
                             [operation.value];
        if (context_frequency == std::numeric_limits<std::uint64_t>::max()
            || field_frequency == std::numeric_limits<std::uint64_t>::max()) {
            result.error = ContextualHuffmanEstimateError::frequency_overflow;
            return result;
        }
        ++context_frequency;
        ++field_frequency;
    }
    result.operation_index = operations.size();

    std::array<Model, context::internal::lzss_field_context_count>
        context_models{};
    for (std::size_t context_id = 0;
         context_id < context_models.size(); ++context_id) {
        const auto error = build_model(
            context_frequencies[context_id],
            context::internal::lzss_field_context_alphabets[context_id],
            context_models[context_id]);
        if (error != ContextualHuffmanEstimateError::none) {
            result.error = error;
            return result;
        }
    }

    auto& field = result.estimates.field_tables;
    field.descriptor_bytes = descriptor_header_bytes;
    field.bypass_bits = bypass_bits;
    constexpr std::array<std::uint16_t, field_table_count> field_alphabets{
        2, 256, 8, 17};
    std::array<Model, field_table_count> field_models{};
    for (std::size_t index = 0; index < field_table_count; ++index) {
        auto& model = field_models[index];
        const auto error = build_model(
            field_frequencies[index], field_alphabets[index], model);
        if (error != ContextualHuffmanEstimateError::none) {
            result.error = error;
            return result;
        }
        if (!model.active) continue;
        ++field.active_tables;
        ++field.stored_models;
        if (!add_u64(field.descriptor_bytes, model.descriptor_bytes,
                     field.descriptor_bytes)
            || !add_u64(field.symbol_bits, model.symbol_bits,
                        field.symbol_bits)) {
            result.error = ContextualHuffmanEstimateError::arithmetic_overflow;
            return result;
        }
    }

    auto& selective = result.estimates.selective_context_tables;
    selective = field;
    for (std::size_t context_id = 0;
         context_id < context_models.size(); ++context_id) {
        const auto& context_model = context_models[context_id];
        if (!context_model.active) continue;
        const auto& base_model = field_models[field_table(
            static_cast<std::uint16_t>(context_id))];
        std::uint64_t base_bits{};
        const auto cost_error = calculate_symbol_bits(
            context_frequencies[context_id], base_model, base_bits);
        if (cost_error != ContextualHuffmanEstimateError::none) {
            result.error = cost_error;
            return result;
        }
        if (base_bits <= context_model.symbol_bits) continue;
        const auto symbol_savings = base_bits - context_model.symbol_bits;
        std::uint64_t descriptor_bits{};
        if (!core::checked_multiply(context_model.descriptor_bytes,
                                    UINT64_C(8), descriptor_bits)) {
            result.error = ContextualHuffmanEstimateError::arithmetic_overflow;
            return result;
        }
        if (symbol_savings <= descriptor_bits) continue;
        selective.symbol_bits -= symbol_savings;
        if (!add_u64(selective.descriptor_bytes,
                     context_model.descriptor_bytes,
                     selective.descriptor_bytes)) {
            result.error = ContextualHuffmanEstimateError::arithmetic_overflow;
            return result;
        }
        ++selective.active_tables;
        ++selective.stored_models;
        ++selective.selected_contexts;
    }

    auto& contextual = result.estimates.contextual_tables;
    contextual.descriptor_bytes = descriptor_header_bytes;
    contextual.bypass_bits = bypass_bits;
    auto& shared = result.estimates.shared_contextual_tables;
    shared.descriptor_bytes = descriptor_header_bytes
        + contextual_model_map_bytes;
    shared.bypass_bits = bypass_bits;
    std::array<std::size_t, context::internal::lzss_field_context_count>
        unique_models{};
    std::size_t unique_count{};
    for (std::size_t context_id = 0;
         context_id < context_models.size(); ++context_id) {
        const auto& model = context_models[context_id];
        if (!model.active) continue;
        ++contextual.active_tables;
        ++contextual.stored_models;
        ++shared.active_tables;
        if (!add_u64(contextual.descriptor_bytes, model.descriptor_bytes,
                     contextual.descriptor_bytes)
            || !add_u64(contextual.symbol_bits, model.symbol_bits,
                        contextual.symbol_bits)
            || !add_u64(shared.symbol_bits, model.symbol_bits,
                        shared.symbol_bits)) {
            result.error = ContextualHuffmanEstimateError::arithmetic_overflow;
            return result;
        }
        bool found{};
        for (std::size_t unique = 0; unique < unique_count; ++unique) {
            if (same_model(model, context_models[unique_models[unique]])) {
                found = true;
                break;
            }
        }
        if (!found) unique_models[unique_count++] = context_id;
    }
    shared.stored_models = static_cast<std::uint16_t>(unique_count);
    for (std::size_t unique = 0; unique < unique_count; ++unique) {
        const auto& model = context_models[unique_models[unique]];
        if (!add_u64(shared.descriptor_bytes, model.descriptor_bytes,
                     shared.descriptor_bytes)) {
            result.error = ContextualHuffmanEstimateError::arithmetic_overflow;
            return result;
        }
    }

    for (auto* estimate : {&field, &selective, &contextual, &shared}) {
        const auto error = finish_estimate(*estimate);
        if (error != ContextualHuffmanEstimateError::none) {
            result.error = error;
            return result;
        }
    }
    return result;
}

} // namespace marc::entropy::internal

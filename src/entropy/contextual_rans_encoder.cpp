#include "entropy/contextual_rans_encoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

struct ModelCounts {
    std::array<std::uint32_t, contextual_rans_frequency_entries> symbols{};
    std::array<std::uint32_t, contextual_rans_context_count> totals{};
};

struct EncodePassResult {
    std::size_t payload_size{};
    ContextualRansEncodeError error{ContextualRansEncodeError::none};
};

[[nodiscard]] ContextualRansEncodeResult fail(
    ContextualRansEncodeResult result,
    const ContextualRansEncodeError error) noexcept {
    result.error = error;
    return result;
}

[[nodiscard]] bool add_decisions(
    const std::uint32_t increment,
    ContextualRansEncodeResult& result) noexcept {
    std::uint32_t updated{};
    if (!core::checked_add(result.decision_count, increment, updated)) {
        result.error = ContextualRansEncodeError::arithmetic_overflow;
        return false;
    }
    result.decision_count = updated;
    return true;
}

[[nodiscard]] ContextualRansEncodeResult validate_and_count(
    const std::span<const ModeledOperation> operations,
    ModelCounts& counts) noexcept {
    ContextualRansEncodeResult result{};
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto kind = static_cast<std::uint8_t>(operation.kind);
        if (kind > static_cast<std::uint8_t>(
                       ModeledOperationKind::bypass_bits)) {
            return fail(result,
                        ContextualRansEncodeError::invalid_operation_kind);
        }
        if (operation.kind == ModeledOperationKind::symbol) {
            if (operation.context_id >= contextual_rans_context_count) {
                return fail(result, ContextualRansEncodeError::invalid_context);
            }
            if (operation.alphabet_size
                != context::internal::lzss_field_context_alphabets[
                    operation.context_id]) {
                return fail(result,
                            ContextualRansEncodeError::invalid_alphabet);
            }
            if (operation.value >= operation.alphabet_size) {
                return fail(result, ContextualRansEncodeError::invalid_symbol);
            }
            if (operation.bit_count != 0) {
                return fail(result,
                            ContextualRansEncodeError::nonzero_unused_field);
            }
            const auto index =
                context::internal::lzss_field_context_offsets[
                    operation.context_id]
                + operation.value;
            if (counts.symbols[index] == UINT32_MAX
                || counts.totals[operation.context_id] == UINT32_MAX) {
                return fail(result,
                            ContextualRansEncodeError::arithmetic_overflow);
            }
            ++counts.symbols[index];
            ++counts.totals[operation.context_id];
            if (!add_decisions(1, result)) return result;
        } else {
            if (operation.context_id != 0 || operation.alphabet_size != 0) {
                return fail(result,
                            ContextualRansEncodeError::nonzero_unused_field);
            }
            if (operation.bit_count == 0 || operation.bit_count > 16) {
                return fail(result,
                            ContextualRansEncodeError::invalid_bypass_width);
            }
            if ((operation.value >> operation.bit_count) != 0) {
                return fail(result,
                            ContextualRansEncodeError::nonzero_unused_field);
            }
            if (!add_decisions(operation.bit_count, result)) return result;
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    return result;
}

[[nodiscard]] std::int64_t normalization_error(
    const std::uint32_t count, const std::uint16_t frequency,
    const std::uint32_t total) noexcept {
    return static_cast<std::int64_t>(count)
            * contextual_rans_total_frequency
        - static_cast<std::int64_t>(frequency) * total;
}

[[nodiscard]] bool normalize_context(
    const std::uint16_t context_id, const ModelCounts& counts,
    std::array<std::uint16_t, contextual_rans_frequency_entries>& frequencies)
    noexcept {
    const auto total = counts.totals[context_id];
    if (total == 0) return true;
    const auto begin =
        context::internal::lzss_field_context_offsets[context_id];
    const auto end =
        context::internal::lzss_field_context_offsets[context_id + 1];
    std::uint32_t sum{};
    for (auto index = begin; index < end; ++index) {
        if (counts.symbols[index] == 0) continue;
        const auto scaled = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(counts.symbols[index])
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
            if (counts.symbols[index] == 0) continue;
            const auto error = normalization_error(
                counts.symbols[index], frequencies[index], total);
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
                counts.symbols[index], frequencies[index], total);
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

[[nodiscard]] bool normalize_models(
    const ModelCounts& counts,
    std::array<std::uint16_t, contextual_rans_frequency_entries>& frequencies)
    noexcept {
    for (std::uint16_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        if (!normalize_context(context_id, counts, frequencies)) return false;
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

[[nodiscard]] bool encode_range(
    const std::uint16_t cumulative, const std::uint16_t frequency,
    std::uint64_t& state, const std::span<std::byte> output,
    std::size_t& cursor, std::size_t& renormalization_bytes,
    ContextualRansEncodeError& error) noexcept {
    if (frequency == 0
        || static_cast<std::uint32_t>(cumulative) + frequency
               > contextual_rans_total_frequency
        || state < rans_lower_bound
        || state >= rans_lower_bound * UINT64_C(256)) {
        error = ContextualRansEncodeError::internal_error;
        return false;
    }
    const auto maximum =
        ((rans_lower_bound >> contextual_rans_table_log) << 8) * frequency;
    while (state >= maximum) {
        if (renormalization_bytes
            == std::numeric_limits<std::size_t>::max()) {
            error = ContextualRansEncodeError::arithmetic_overflow;
            return false;
        }
        if (!output.empty()) {
            if (cursor <= rans_min_payload_size) {
                error = ContextualRansEncodeError::internal_error;
                return false;
            }
            output[--cursor] = static_cast<std::byte>(state & 0xffU);
        }
        ++renormalization_bytes;
        state >>= 8;
    }
    std::uint64_t quotient_part{};
    if (!core::checked_multiply(
            state / frequency,
            static_cast<std::uint64_t>(contextual_rans_total_frequency),
            quotient_part)
        || !core::checked_add(quotient_part, state % frequency, state)
        || !core::checked_add(
            state, static_cast<std::uint64_t>(cumulative), state)) {
        error = ContextualRansEncodeError::arithmetic_overflow;
        return false;
    }
    return true;
}

[[nodiscard]] EncodePassResult encode_pass(
    const std::span<const ModeledOperation> operations,
    const ContextualRansDescriptor& descriptor,
    const std::span<std::byte> output) noexcept {
    std::uint64_t state = rans_lower_bound;
    std::size_t cursor = output.size();
    std::size_t renormalization_bytes{};
    ContextualRansEncodeError error{};
    for (std::size_t reverse = operations.size(); reverse != 0; --reverse) {
        const auto& operation = operations[reverse - 1];
        if (operation.kind == ModeledOperationKind::symbol) {
            const auto offset =
                context::internal::lzss_field_context_offsets[
                    operation.context_id];
            if (!encode_range(
                    cumulative_for(descriptor, operation.context_id,
                                   operation.value),
                    descriptor.frequencies[offset + operation.value], state,
                    output, cursor, renormalization_bytes, error)) {
                return {0, error};
            }
        } else {
            for (std::uint8_t reverse_bit = operation.bit_count;
                 reverse_bit != 0; --reverse_bit) {
                const auto bit = static_cast<std::uint16_t>(
                    (operation.value >> (reverse_bit - 1)) & 1U);
                if (!encode_range(
                        static_cast<std::uint16_t>(
                            bit * (contextual_rans_total_frequency / 2U)),
                        static_cast<std::uint16_t>(
                            contextual_rans_total_frequency / 2U),
                        state, output, cursor, renormalization_bytes, error)) {
                    return {0, error};
                }
            }
        }
    }
    if (state < rans_lower_bound
        || state >= rans_lower_bound * UINT64_C(256)) {
        return {0, ContextualRansEncodeError::internal_error};
    }
    std::size_t payload_size{};
    if (!core::checked_add(
            static_cast<std::size_t>(rans_min_payload_size),
            renormalization_bytes, payload_size)) {
        return {0, ContextualRansEncodeError::arithmetic_overflow};
    }
    if (!output.empty()
        && (output.size() != payload_size || cursor != rans_min_payload_size
            || !core::store_le(output, 0, state))) {
        return {payload_size, ContextualRansEncodeError::internal_error};
    }
    return {payload_size, ContextualRansEncodeError::none};
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck operation_payload_overlap(
    const std::span<const ModeledOperation> operations,
    const std::span<std::byte> payload) noexcept {
    if (operations.empty() || payload.empty()) return OverlapCheck::disjoint;
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(ModeledOperation), operation_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    const auto payload_begin = reinterpret_cast<std::uintptr_t>(payload.data());
    std::uintptr_t operation_end{};
    std::uintptr_t payload_end{};
    if (!core::checked_add(
            operation_begin, static_cast<std::uintptr_t>(operation_bytes),
            operation_end)
        || !core::checked_add(
            payload_begin, static_cast<std::uintptr_t>(payload.size()),
            payload_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return operation_begin < payload_end && payload_begin < operation_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

ContextualRansEncodeResult plan_contextual_rans_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept {
    if (operations.empty()) {
        return {0, 0, 0, 0, ContextualRansEncodeError::empty_operations};
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || contextual_rans_decode_table_entries
               > limits.max_entropy_table_entries) {
        return {0, 0, 0, 0, ContextualRansEncodeError::limit_exceeded};
    }
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(ModeledOperation), operation_bytes)) {
        return {0, 0, 0, 0,
                ContextualRansEncodeError::arithmetic_overflow};
    }
    if (operation_bytes > limits.max_internal_buffered_bytes) {
        return {0, 0, 0, 0, ContextualRansEncodeError::limit_exceeded};
    }

    ModelCounts counts{};
    auto result = validate_and_count(operations, counts);
    if (result.error != ContextualRansEncodeError::none) return result;

    ContextualRansDescriptor planned{};
    if (!normalize_models(counts, planned.frequencies)) {
        return fail(result, ContextualRansEncodeError::normalization_error);
    }
    const auto pass = encode_pass(operations, planned, {});
    result.payload_size = pass.payload_size;
    if (pass.error != ContextualRansEncodeError::none) {
        return fail(result, pass.error);
    }
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result, ContextualRansEncodeError::arithmetic_overflow);
    }
    planned.decision_count = result.decision_count;
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    const auto format_error = validate_contextual_rans_descriptor(
        planned, planned.decision_count, planned.payload_size, limits);
    if (format_error == ContextualRansFormatError::limit_exceeded) {
        return fail(result, ContextualRansEncodeError::limit_exceeded);
    }
    if (format_error == ContextualRansFormatError::arithmetic_overflow) {
        return fail(result, ContextualRansEncodeError::arithmetic_overflow);
    }
    if (format_error != ContextualRansFormatError::none) {
        return fail(result, ContextualRansEncodeError::internal_error);
    }
    descriptor = planned;
    return result;
}

ContextualRansEncodeResult encode_contextual_rans_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    ContextualRansDescriptor& descriptor) noexcept {
    ContextualRansDescriptor planned{};
    const auto plan =
        plan_contextual_rans_operations(operations, limits, planned);
    if (plan.error != ContextualRansEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return fail(plan,
                    ContextualRansEncodeError::payload_output_too_small);
    }
    const auto output = payload_output.first(plan.payload_size);
    const auto overlap = operation_payload_overlap(operations, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        return fail(plan, ContextualRansEncodeError::arithmetic_overflow);
    }
    if (overlap == OverlapCheck::overlap) {
        return fail(plan, ContextualRansEncodeError::overlapping_buffers);
    }
    const auto encoded = encode_pass(operations, planned, output);
    if (encoded.error != ContextualRansEncodeError::none
        || encoded.payload_size != plan.payload_size) {
        return fail(plan, ContextualRansEncodeError::internal_error);
    }
    descriptor = planned;
    return plan;
}

} // namespace marc::entropy::internal

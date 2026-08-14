#include "entropy/contextual_tans_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

[[nodiscard]] ContextualTansEncodeResult fail(
    ContextualTansEncodeResult result,
    const ContextualTansEncodeError error) noexcept {
    result.error = error;
    return result;
}

[[nodiscard]] ContextualTansEncodeError add_operation(
    ContextualTansModelBuilder& builder,
    const ModeledOperation& operation) noexcept {
    const auto kind = static_cast<std::uint8_t>(operation.kind);
    if (kind > static_cast<std::uint8_t>(
                   ModeledOperationKind::bypass_bits)) {
        return ContextualTansEncodeError::invalid_operation_kind;
    }
    if (operation.kind == ModeledOperationKind::symbol) {
        if (operation.bit_count != 0) {
            return ContextualTansEncodeError::nonzero_unused_field;
        }
        return builder.add_symbol(
            operation.context_id, operation.alphabet_size, operation.value);
    }
    if (operation.context_id != 0 || operation.alphabet_size != 0) {
        return ContextualTansEncodeError::nonzero_unused_field;
    }
    return builder.add_bypass(operation.bit_count, operation.value);
}

[[nodiscard]] ContextualTansEncodeError encode_operation(
    ContextualTansReverseWriter& writer,
    const ModeledOperation& operation) noexcept {
    return operation.kind == ModeledOperationKind::symbol
        ? writer.encode_symbol(
            operation.context_id, operation.alphabet_size, operation.value)
        : writer.encode_bypass(operation.bit_count, operation.value);
}

[[nodiscard]] ContextualTansEncodeResult plan_model(
    const std::span<const ModeledOperation> operations,
    ContextualTansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualTansEncodeResult result{};
    ContextualTansModelBuilder builder{variant};
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto error = add_operation(builder, operation);
        if (error != ContextualTansEncodeError::none) {
            return fail(result, error);
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    const auto error = builder.finish(descriptor);
    result.decision_count = builder.decision_count();
    if (error != ContextualTansEncodeError::none) {
        return fail(result, error);
    }
    return result;
}

[[nodiscard]] ContextualTansEncodeError run_reverse(
    const std::span<const ModeledOperation> operations,
    const ContextualTansDescriptor& descriptor,
    const std::span<const std::uint16_t> tables,
    const std::span<std::byte> output,
    std::size_t& payload_size,
    std::uint8_t& final_valid_bits,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualTansReverseWriter writer(descriptor, tables, output, variant);
    for (std::size_t reverse = operations.size(); reverse != 0; --reverse) {
        const auto error = encode_operation(writer, operations[reverse - 1]);
        if (error != ContextualTansEncodeError::none) return error;
    }
    return writer.finish(payload_size, final_valid_bits);
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(
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

[[nodiscard]] ContextualTansEncodeResult fail_overlap(
    ContextualTansEncodeResult result,
    const OverlapCheck overlap) noexcept {
    return fail(
        result, overlap == OverlapCheck::arithmetic_overflow
            ? ContextualTansEncodeError::arithmetic_overflow
            : ContextualTansEncodeError::overlapping_buffers);
}

} // namespace

ContextualTansEncodeResult plan_contextual_tans_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::uint16_t> private_encode_tables,
    ContextualTansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualTansEncodeResult result{};
    if (operations.empty()) {
        return fail(result, ContextualTansEncodeError::empty_operations);
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || contextual_tans_encode_table_entries
               > limits.max_entropy_table_entries) {
        return fail(result, ContextualTansEncodeError::limit_exceeded);
    }
    std::size_t operation_bytes{};
    std::size_t table_capacity_bytes{};
    std::size_t table_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(ModeledOperation), operation_bytes)
        || !core::checked_multiply(
            private_encode_tables.size(), sizeof(std::uint16_t),
            table_capacity_bytes)
        || !core::checked_multiply(
            contextual_tans_encode_table_entries, sizeof(std::uint16_t),
            table_bytes)) {
        return fail(result, ContextualTansEncodeError::arithmetic_overflow);
    }
    const auto operation_table_overlap = regions_overlap(
        operations.data(), operation_bytes, private_encode_tables.data(),
        table_capacity_bytes);
    if (operation_table_overlap != OverlapCheck::disjoint) {
        return fail_overlap(result, operation_table_overlap);
    }
    if (private_encode_tables.size()
        < contextual_tans_encode_table_entries) {
        return fail(result, ContextualTansEncodeError::table_output_too_small);
    }
    std::size_t minimum_buffered{};
    if (!core::checked_add(
            operation_bytes, table_bytes, minimum_buffered)) {
        return fail(result, ContextualTansEncodeError::arithmetic_overflow);
    }
    if (minimum_buffered > limits.max_internal_buffered_bytes) {
        return fail(result, ContextualTansEncodeError::limit_exceeded);
    }
    ContextualTansDescriptor planned{};
    result = plan_model(operations, planned, variant);
    if (result.error != ContextualTansEncodeError::none) return result;
    const auto table_error = build_contextual_tans_encode_tables(
        planned, limits, private_encode_tables, variant);
    if (table_error != ContextualTansEncodeError::none) {
        return fail(result, table_error);
    }
    std::uint8_t final_valid_bits{};
    const auto encode_error = run_reverse(
        operations, planned,
        private_encode_tables.first(contextual_tans_encode_table_entries), {},
        result.payload_size, final_valid_bits, variant);
    if (encode_error != ContextualTansEncodeError::none) {
        return fail(result, encode_error);
    }
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result, ContextualTansEncodeError::arithmetic_overflow);
    }
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    planned.final_valid_bits = final_valid_bits;
    std::size_t descriptor_size{};
    const auto format_error = validate_contextual_tans_descriptor(
        planned, planned.decision_count, planned.payload_size, limits,
        descriptor_size, variant);
    if (format_error == ContextualTansFormatError::limit_exceeded) {
        return fail(result, ContextualTansEncodeError::limit_exceeded);
    }
    if (format_error == ContextualTansFormatError::arithmetic_overflow) {
        return fail(result, ContextualTansEncodeError::arithmetic_overflow);
    }
    if (format_error != ContextualTansFormatError::none) {
        return fail(result, ContextualTansEncodeError::internal_error);
    }
    std::size_t buffered{};
    if (!core::checked_add(
            minimum_buffered, result.payload_size, buffered)) {
        return fail(result, ContextualTansEncodeError::arithmetic_overflow);
    }
    if (buffered > limits.max_internal_buffered_bytes) {
        return fail(result, ContextualTansEncodeError::limit_exceeded);
    }
    descriptor = planned;
    return result;
}

ContextualTansEncodeResult encode_contextual_tans_operations(
    const std::span<const ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> payload_output,
    ContextualTansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualTansEncodeResult initial{};
    std::size_t operation_bytes{};
    std::size_t table_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(ModeledOperation), operation_bytes)
        || !core::checked_multiply(
            private_encode_tables.size(), sizeof(std::uint16_t),
            table_bytes)) {
        return fail(initial, ContextualTansEncodeError::arithmetic_overflow);
    }
    for (const auto overlap : {
             regions_overlap(
                 operations.data(), operation_bytes, payload_output.data(),
                 payload_output.size()),
             regions_overlap(
                 private_encode_tables.data(), table_bytes,
                 payload_output.data(), payload_output.size())}) {
        if (overlap != OverlapCheck::disjoint) {
            return fail_overlap(initial, overlap);
        }
    }
    ContextualTansDescriptor planned{};
    auto result = plan_contextual_tans_operations(
        operations, limits, private_encode_tables, planned, variant);
    if (result.error != ContextualTansEncodeError::none) return result;
    if (payload_output.size() < result.payload_size) {
        return fail(result,
                    ContextualTansEncodeError::payload_output_too_small);
    }
    const auto output = payload_output.first(result.payload_size);
    std::fill(output.begin(), output.end(), std::byte{});
    std::size_t encoded_size{};
    std::uint8_t final_valid_bits{};
    const auto error = run_reverse(
        operations, planned,
        private_encode_tables.first(contextual_tans_encode_table_entries),
        output, encoded_size, final_valid_bits, variant);
    if (error != ContextualTansEncodeError::none
        || encoded_size != result.payload_size
        || final_valid_bits != planned.final_valid_bits) {
        return fail(result, ContextualTansEncodeError::internal_error);
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal

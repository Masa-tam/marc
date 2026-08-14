#include "entropy/contextual_rans_encoder.hpp"

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

using context::internal::ModeledOperation;
using context::internal::ModeledOperationKind;

[[nodiscard]] ContextualRansEncodeResult fail(
    ContextualRansEncodeResult result,
    const ContextualRansEncodeError error) noexcept {
    result.error = error;
    return result;
}

[[nodiscard]] ContextualRansEncodeError add_operation(
    ContextualRansModelBuilder& builder,
    const ModeledOperation& operation) noexcept {
    const auto kind = static_cast<std::uint8_t>(operation.kind);
    if (kind > static_cast<std::uint8_t>(
                   ModeledOperationKind::bypass_bits)) {
        return ContextualRansEncodeError::invalid_operation_kind;
    }
    if (operation.kind == ModeledOperationKind::symbol) {
        if (operation.bit_count != 0) {
            return ContextualRansEncodeError::nonzero_unused_field;
        }
        return builder.add_symbol(
            operation.context_id, operation.alphabet_size, operation.value);
    }
    if (operation.context_id != 0 || operation.alphabet_size != 0) {
        return ContextualRansEncodeError::nonzero_unused_field;
    }
    return builder.add_bypass(operation.bit_count, operation.value);
}

[[nodiscard]] ContextualRansEncodeError encode_operation(
    ContextualRansReverseWriter& writer,
    const ModeledOperation& operation) noexcept {
    return operation.kind == ModeledOperationKind::symbol
        ? writer.encode_symbol(
            operation.context_id, operation.alphabet_size, operation.value)
        : writer.encode_bypass(operation.bit_count, operation.value);
}

[[nodiscard]] ContextualRansEncodeResult plan_model(
    const std::span<const ModeledOperation> operations,
    ContextualRansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualRansEncodeResult result{};
    ContextualRansModelBuilder builder{variant};
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto error = add_operation(builder, operation);
        if (error != ContextualRansEncodeError::none) {
            return fail(result, error);
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    const auto error = builder.finish(descriptor);
    result.decision_count = builder.decision_count();
    if (error != ContextualRansEncodeError::none) {
        return fail(result, error);
    }
    return result;
}

[[nodiscard]] ContextualRansEncodeError run_reverse(
    const std::span<const ModeledOperation> operations,
    const ContextualRansDescriptor& descriptor,
    const std::span<std::byte> output,
    std::size_t& payload_size,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualRansReverseWriter writer(descriptor, output, variant);
    for (std::size_t reverse = operations.size(); reverse != 0; --reverse) {
        const auto error = encode_operation(writer, operations[reverse - 1]);
        if (error != ContextualRansEncodeError::none) return error;
    }
    return writer.finish(payload_size);
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
    ContextualRansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
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

    ContextualRansDescriptor planned{};
    auto result = plan_model(operations, planned, variant);
    if (result.error != ContextualRansEncodeError::none) return result;
    const auto encode_error = run_reverse(
        operations, planned, {}, result.payload_size, variant);
    if (encode_error != ContextualRansEncodeError::none) {
        return fail(result, encode_error);
    }
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result, ContextualRansEncodeError::arithmetic_overflow);
    }
    planned.payload_size = static_cast<std::uint32_t>(result.payload_size);
    std::size_t descriptor_size{};
    const auto format_error = validate_contextual_rans_descriptor(
        planned, planned.decision_count, planned.payload_size, limits,
        descriptor_size, variant);
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
    ContextualRansDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualRansDescriptor planned{};
    const auto plan =
        plan_contextual_rans_operations(operations, limits, planned, variant);
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
    std::size_t encoded_size{};
    const auto error = run_reverse(
        operations, planned, output, encoded_size, variant);
    if (error != ContextualRansEncodeError::none
        || encoded_size != plan.payload_size) {
        return fail(plan, ContextualRansEncodeError::internal_error);
    }
    descriptor = planned;
    return plan;
}

} // namespace marc::entropy::internal

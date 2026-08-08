#ifndef MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_dynamic_range_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualDynamicRangeEncodeError : std::uint8_t {
    none,
    empty_operations,
    invalid_operation_kind,
    invalid_context,
    invalid_alphabet,
    invalid_symbol,
    invalid_bypass_width,
    nonzero_unused_field,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct ContextualDynamicRangeEncodeResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    ContextualDynamicRangeEncodeError error{
        ContextualDynamicRangeEncodeError::none};
};

[[nodiscard]] ContextualDynamicRangeEncodeResult
plan_contextual_dynamic_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult
encode_contextual_dynamic_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

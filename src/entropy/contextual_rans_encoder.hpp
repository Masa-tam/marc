#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualRansEncodeError : std::uint8_t {
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
    normalization_error,
    arithmetic_overflow,
    internal_error,
};

struct ContextualRansEncodeResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    ContextualRansEncodeError error{ContextualRansEncodeError::none};
};

[[nodiscard]] ContextualRansEncodeResult plan_contextual_rans_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualRansEncodeResult encode_contextual_rans_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    ContextualRansDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

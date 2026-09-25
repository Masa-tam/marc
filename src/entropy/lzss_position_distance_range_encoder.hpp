#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_ENCODER_HPP

#include "entropy/contextual_dynamic_range_encoder.hpp"

namespace marc::entropy::internal {

// Private grammar-aware encoder; history/frame validation and public admission
// are separate. Invalid or incomplete field sequences return invalid_symbol.
// Operations must remain stable during planning and writing. Preflight errors
// leave descriptor and output untouched; output is consumable only on success.
// Memory charge: operation bytes + this fixed model/writer/cursor state + payload bytes.
// This excludes caller-unused capacity and transient scalar call-stack overhead.
[[nodiscard]] std::size_t lzss_position_distance_range_encoder_state_bytes() noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult plan_lzss_position_distance_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, ContextualDynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult encode_lzss_position_distance_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

// Retained generic model-update path for private differential tests/benchmarks.
// Same validation and memory contract; not a public codec API or format variant.
[[nodiscard]] ContextualDynamicRangeEncodeResult plan_lzss_position_distance_range_operations_reference(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, ContextualDynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult encode_lzss_position_distance_range_operations_reference(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal
#endif

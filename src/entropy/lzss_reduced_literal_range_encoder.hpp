#ifndef MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_ENCODER_HPP

#include "entropy/contextual_dynamic_range_encoder.hpp"

namespace marc::entropy::internal {

// Private operation encoder, not token-grammar validation or public admission.
// Operations must remain stable during planning and writing. Preflight errors
// leave descriptor and output untouched; output is consumable only on success.
// Memory charge: operation bytes + this fixed model/writer state + payload bytes.
// This excludes caller-unused capacity and transient scalar call-stack overhead.
[[nodiscard]] std::size_t lzss_reduced_literal_range_encoder_state_bytes() noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult plan_lzss_reduced_literal_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, ContextualDynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult encode_lzss_reduced_literal_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal
#endif

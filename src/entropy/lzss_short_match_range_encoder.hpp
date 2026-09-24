#ifndef MARC_ENTROPY_LZSS_SHORT_MATCH_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_SHORT_MATCH_RANGE_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_dynamic_range_encoder.hpp"

namespace marc::entropy::internal {

// Private 32-context Format 2.0 encoder. Neither entry point admits the
// reserved identity through a public stream or C API. Operations must remain
// stable across the planning and writing passes of encode.
[[nodiscard]] ContextualDynamicRangeEncodeResult
plan_lzss_short_match_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualDynamicRangeEncodeResult
encode_lzss_short_match_range_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

#ifndef MARC_CONTEXT_LZSS_SHORT_MATCH_OPERATIONS_HPP
#define MARC_CONTEXT_LZSS_SHORT_MATCH_OPERATIONS_HPP

#include "context/lzss_field_context.hpp"

namespace marc::context::internal {

// Private variant-6 forward mapping. A complete typed frame is validated
// before any caller-owned operation storage is written.
[[nodiscard]] LzssFieldContextResult plan_lzss_short_match_operations(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFieldContextResult model_lzss_short_match_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<ModeledOperation> operations) noexcept;

} // namespace marc::context::internal

#endif

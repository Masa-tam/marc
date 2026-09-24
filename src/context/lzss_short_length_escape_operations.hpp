#ifndef MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_OPERATIONS_HPP
#define MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_OPERATIONS_HPP

#include "context/lzss_short_match_operations.hpp"

namespace marc::context::internal {

// Private 2/8 + 1/7 forward mapping. The whole typed frame is validated
// before any caller-owned operation storage is written.
[[nodiscard]] LzssFieldContextResult plan_lzss_short_length_escape_operations(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFieldContextResult model_lzss_short_length_escape_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<ModeledOperation> operations) noexcept;

} // namespace marc::context::internal

#endif

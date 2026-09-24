#ifndef MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_OPERATION_DECODER_HPP
#define MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_OPERATION_DECODER_HPP

#include "context/lzss_field_context.hpp"

namespace marc::context::internal {

// Private dictionary 2/8 + context 1/7 operation validator. No stream
// identity is admitted by these functions.
[[nodiscard]] LzssFieldContextResult
validate_lzss_short_length_escape_operations(
    std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFieldContextResult
invert_lzss_short_length_escape_operations(
    std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

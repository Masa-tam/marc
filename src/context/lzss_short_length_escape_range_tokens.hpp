#ifndef MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_RANGE_TOKENS_HPP
#define MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_RANGE_TOKENS_HPP

#include "context/lzss_contextual_range_decoder.hpp"

namespace marc::context::internal {

// Private dictionary 2/8 + context 1/7 + entropy 3/2 path only. These
// entry points do not admit the identity through a stream or public API.
[[nodiscard]] LzssContextualRangeDecodeResult
validate_lzss_short_length_escape_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssContextualRangeDecodeResult
decode_lzss_short_length_escape_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

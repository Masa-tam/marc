#ifndef MARC_CONTEXT_LZSS_SHORT_MATCH_RANGE_TOKENS_HPP
#define MARC_CONTEXT_LZSS_SHORT_MATCH_RANGE_TOKENS_HPP

#include "context/lzss_contextual_range_decoder.hpp"

namespace marc::context::internal {

// Private variant-7/6 token path. It does not admit the reserved stream
// through any published parser or frame decoder.
[[nodiscard]] LzssContextualRangeDecodeResult
validate_lzss_short_match_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssContextualRangeDecodeResult
decode_lzss_short_match_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_1M_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_1M_TOKENS_HPP

#include "context/lzss_contextual_range_decoder.hpp"

namespace marc::context::internal {

// Private dictionary 2/9 + context 1/10 + entropy 3/2 bridge. No frame or
// public stream admission. Input/configuration must remain stable during calls.
[[nodiscard]] LzssFieldContextResult plan_lzss_position_distance_1m_operations(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

// Validate the complete frame, capacity and overlap before writing operations.
[[nodiscard]] LzssFieldContextResult model_lzss_position_distance_1m_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits, std::span<ModeledOperation> operations) noexcept;

[[nodiscard]] LzssContextualRangeDecodeResult validate_lzss_position_distance_1m_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

// Two-pass transactional output. Storage failures follow payload validation.
[[nodiscard]] LzssContextualRangeDecodeResult decode_lzss_position_distance_1m_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> tokens) noexcept;

// A failed call may leave a validated prefix in discardable private scratch.
// Never reconstruct or publish it unless the entire call succeeds. Invalid
// capacity/overlap uses the transactional path, retaining its error precedence.
[[nodiscard]] LzssContextualRangeDecodeResult decode_lzss_position_distance_1m_token_scratch(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> scratch) noexcept;

} // namespace marc::context::internal
#endif

#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_RANGE_DECODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_RANGE_DECODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualRangeDecodeError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_counts,
    entropy_error,
    invalid_token,
    raw_size_mismatch,
    output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualRangeDecodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t raw_size{};
    entropy::internal::ContextualDynamicRangeDecodeResult entropy{};
    dictionary::internal::LzssTypedTokenError token_error{
        dictionary::internal::LzssTypedTokenError::none};
    LzssContextualRangeDecodeError error{
        LzssContextualRangeDecodeError::none};
};

[[nodiscard]] LzssContextualRangeDecodeResult
validate_lzss_contextual_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualRangeDecodeResult
decode_lzss_contextual_range_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::context::internal

#endif

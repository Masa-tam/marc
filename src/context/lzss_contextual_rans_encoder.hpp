#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_RANS_ENCODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_RANS_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_rans_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualRansEncodeError : std::uint8_t {
    none,
    token_validation_error,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    entropy_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualRansEncodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t descriptor_size{};
    dictionary::internal::LzssTypedFrameValidationResult token_validation{};
    entropy::internal::ContextualRansEncodeError entropy_error{
        entropy::internal::ContextualRansEncodeError::none};
    LzssContextualRansEncodeError error{
        LzssContextualRansEncodeError::none};
};

[[nodiscard]] LzssContextualRansEncodeResult
plan_lzss_contextual_rans_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualRansDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualRansEncodeResult
encode_lzss_contextual_rans_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    entropy::internal::ContextualRansDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::context::internal

#endif

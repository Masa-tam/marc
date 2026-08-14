#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_TANS_ENCODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_TANS_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_tans_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualTansEncodeError : std::uint8_t {
    none,
    token_validation_error,
    table_output_too_small,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    entropy_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualTansEncodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t required_table_entries{
        entropy::internal::contextual_tans_encode_table_entries};
    dictionary::internal::LzssTypedFrameValidationResult token_validation{};
    entropy::internal::ContextualTansEncodeError entropy_error{
        entropy::internal::ContextualTansEncodeError::none};
    LzssContextualTansEncodeError error{
        LzssContextualTansEncodeError::none};
};

[[nodiscard]] LzssContextualTansEncodeResult
plan_lzss_contextual_tans_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::uint16_t> private_encode_tables,
    entropy::internal::ContextualTansDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualTansEncodeResult
encode_lzss_contextual_tans_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::uint16_t> private_encode_tables,
    std::span<std::byte> payload_output,
    entropy::internal::ContextualTansDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::context::internal

#endif

#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_TANS_DECODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_TANS_DECODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_tans_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualTansDecodeError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_counts,
    entropy_error,
    invalid_token,
    raw_size_mismatch,
    table_output_too_small,
    token_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualTansDecodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t raw_size{};
    entropy::internal::ContextualTansDecodeResult entropy{};
    dictionary::internal::LzssTypedTokenError token_error{
        dictionary::internal::LzssTypedTokenError::none};
    LzssContextualTansDecodeError error{
        LzssContextualTansDecodeError::none};
};

[[nodiscard]] LzssContextualTansDecodeResult
validate_lzss_contextual_tans_tokens(
    const entropy::internal::ContextualTansDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::TansDecodeEntry> private_tables) noexcept;

[[nodiscard]] LzssContextualTansDecodeResult
decode_lzss_contextual_tans_tokens(
    const entropy::internal::ContextualTansDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::TansDecodeEntry> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

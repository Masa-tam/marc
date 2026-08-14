#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_blocked_huffman_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualBlockedHuffmanEncodeError : std::uint8_t {
    none,
    token_validation_error,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    entropy_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualBlockedHuffmanEncodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    dictionary::internal::LzssTypedFrameValidationResult token_validation{};
    entropy::internal::ContextualBlockedHuffmanEncodeError entropy_error{
        entropy::internal::ContextualBlockedHuffmanEncodeError::none};
    LzssContextualBlockedHuffmanEncodeError error{
        LzssContextualBlockedHuffmanEncodeError::none};
};

[[nodiscard]] LzssContextualBlockedHuffmanEncodeResult
plan_lzss_contextual_blocked_huffman_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanEncodeResult
encode_lzss_contextual_blocked_huffman_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits, std::span<std::byte> payload_output,
    entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::context::internal

#endif

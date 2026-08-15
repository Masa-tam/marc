#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_ENCODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_adaptive_huffman_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualAdaptiveHuffmanEncodeError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_token,
    node_workspace_too_small,
    symbol_workspace_too_small,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    entropy_error,
    internal_error,
};

struct LzssContextualAdaptiveHuffmanEncodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t payload_bits{};
    dictionary::internal::LzssTypedFrameValidationResult token_validation{};
    entropy::internal::ContextualAdaptiveHuffmanEncodeResult entropy{};
    LzssContextualAdaptiveHuffmanEncodeError error{
        LzssContextualAdaptiveHuffmanEncodeError::none};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanEncodeResult
plan_lzss_contextual_adaptive_huffman_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::AdaptiveHuffmanNode> node_workspace,
    std::span<std::uint16_t> symbol_workspace,
    entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanEncodeResult
encode_lzss_contextual_adaptive_huffman_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::AdaptiveHuffmanNode> node_workspace,
    std::span<std::uint16_t> symbol_workspace,
    std::span<std::byte> payload_output,
    entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k)
    noexcept;

} // namespace marc::context::internal

#endif

#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_DECODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_DECODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_adaptive_huffman_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualAdaptiveHuffmanDecodeError : std::uint8_t {
    none,
    invalid_parameters,
    invalid_counts,
    entropy_error,
    invalid_token,
    raw_size_mismatch,
    node_workspace_too_small,
    symbol_workspace_too_small,
    token_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualAdaptiveHuffmanDecodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t raw_size{};
    entropy::internal::ContextualAdaptiveHuffmanDecodeResult entropy{};
    dictionary::internal::LzssTypedTokenError token_error{
        dictionary::internal::LzssTypedTokenError::none};
    LzssContextualAdaptiveHuffmanDecodeError error{
        LzssContextualAdaptiveHuffmanDecodeError::none};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanDecodeResult
validate_lzss_contextual_adaptive_huffman_tokens(
    const entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanDecodeResult
decode_lzss_contextual_adaptive_huffman_tokens(
    const entropy::internal::ContextualAdaptiveHuffmanDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::context::internal

#endif

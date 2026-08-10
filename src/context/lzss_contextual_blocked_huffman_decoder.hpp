#ifndef MARC_CONTEXT_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_DECODER_HPP
#define MARC_CONTEXT_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_DECODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_blocked_huffman_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class LzssContextualBlockedHuffmanDecodeError : std::uint8_t {
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

struct LzssContextualBlockedHuffmanDecodeResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t raw_size{};
    entropy::internal::ContextualBlockedHuffmanDecodeResult entropy{};
    dictionary::internal::LzssTypedTokenError token_error{
        dictionary::internal::LzssTypedTokenError::none};
    LzssContextualBlockedHuffmanDecodeError error{
        LzssContextualBlockedHuffmanDecodeError::none};
};

[[nodiscard]] LzssContextualBlockedHuffmanDecodeResult
validate_lzss_contextual_blocked_huffman_tokens(
    const entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables) noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanDecodeResult
decode_lzss_contextual_blocked_huffman_tokens(
    const entropy::internal::ContextualBlockedHuffmanDescriptor& descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

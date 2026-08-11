#ifndef MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_DECODER_HPP

#include "context/lzss_contextual_adaptive_huffman_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/lzss_contextual_adaptive_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualAdaptiveHuffmanFrameDecodeError : std::uint8_t {
    none,
    preflight_error,
    output_size_unsupported,
    node_workspace_too_small,
    symbol_workspace_too_small,
    token_output_too_small,
    raw_output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    token_decode_error,
    reconstruction_error,
};

struct LzssContextualAdaptiveHuffmanFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_node_entries{};
    std::size_t required_symbol_entries{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    LzssContextualAdaptiveHuffmanFramePreflightResult preflight{};
    context::internal::LzssContextualAdaptiveHuffmanDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssContextualAdaptiveHuffmanFrameDecodeError error{
        LzssContextualAdaptiveHuffmanFrameDecodeError::none};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameDecodeResult
decode_lzss_contextual_adaptive_huffman_frame(
    std::span<const std::byte> serialized_frame,
    const LzssContextualAdaptiveHuffmanFrameValidationContext& context,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif

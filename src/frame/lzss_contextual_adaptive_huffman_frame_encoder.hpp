#ifndef MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_ENCODER_HPP

#include "context/lzss_contextual_adaptive_huffman_encoder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualAdaptiveHuffmanFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    input_size_mismatch,
    token_staging_too_small,
    node_staging_too_small,
    symbol_staging_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    token_encode_error,
    entropy_encode_error,
    header_error,
    descriptor_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualAdaptiveHuffmanFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t descriptor_size{};
    std::size_t token_count{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t required_node_entries{
        entropy::internal::contextual_adaptive_huffman_node_entries};
    std::size_t required_symbol_entries{
        entropy::internal::contextual_adaptive_huffman_symbol_entries};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssContextualAdaptiveHuffmanEncodeResult
        entropy_encode{};
    LzssContextualAdaptiveHuffmanFrameHeaderError header_error{
        LzssContextualAdaptiveHuffmanFrameHeaderError::none};
    entropy::internal::ContextualAdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::ContextualAdaptiveHuffmanFormatError::none};
    LzssContextualAdaptiveHuffmanFrameEncodeError error{
        LzssContextualAdaptiveHuffmanFrameEncodeError::none};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult
plan_lzss_contextual_adaptive_huffman_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols) noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult
encode_lzss_contextual_adaptive_huffman_frame(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    std::span<std::byte> serialized_output) noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult
plan_lzss_contextual_adaptive_huffman_frame_hash_chain(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanFrameEncodeResult
encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<entropy::internal::AdaptiveHuffmanNode> private_nodes,
    std::span<std::uint16_t> private_symbols,
    std::span<std::byte> match_finder_workspace,
    std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

} // namespace marc::frame::internal

#endif

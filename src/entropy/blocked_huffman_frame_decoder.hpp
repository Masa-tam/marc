#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_FRAME_DECODER_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_FRAME_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class BlockedHuffmanFrameDecodeError : std::uint8_t {
    none,
    empty_views,
    invalid_view,
    output_too_small,
    output_limit,
    block_error,
    arithmetic_overflow,
    internal_error,
};

struct BlockedHuffmanFrameDecodeResult {
    std::size_t output_size{};
    std::size_t block_index{};
    BlockedHuffmanDecodeError block_error{BlockedHuffmanDecodeError::none};
    BlockedHuffmanFrameDecodeError error{
        BlockedHuffmanFrameDecodeError::none};
};

[[nodiscard]] BlockedHuffmanFrameDecodeResult
decode_blocked_huffman_frame(
    std::span<const std::byte> descriptor_region,
    std::span<const std::byte> payload_region,
    std::span<const BlockedHuffmanBlockView> views,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif

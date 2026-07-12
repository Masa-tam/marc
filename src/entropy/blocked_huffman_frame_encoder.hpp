#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_FRAME_ENCODER_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_FRAME_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class BlockedHuffmanFrameEncodeError : std::uint8_t {
    none,
    empty_input,
    invalid_block_size,
    input_limit,
    descriptor_output_too_small,
    payload_output_too_small,
    block_error,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct BlockedHuffmanFrameEncodeResult {
    std::size_t block_count{};
    std::size_t descriptor_region_size{};
    std::size_t payload_size{};
    std::size_t block_index{};
    BlockedHuffmanEncodeError block_error{BlockedHuffmanEncodeError::none};
    BlockedHuffmanFrameEncodeError error{
        BlockedHuffmanFrameEncodeError::none};
};

[[nodiscard]] BlockedHuffmanFrameEncodeResult
plan_blocked_huffman_frame(
    std::span<const std::byte> input,
    std::uint32_t block_size,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] BlockedHuffmanFrameEncodeResult
encode_blocked_huffman_frame(
    std::span<const std::byte> input,
    std::uint32_t block_size,
    const core::DecoderLimits& limits,
    std::span<std::byte> descriptor_output,
    std::span<std::byte> payload_output) noexcept;

} // namespace marc::entropy::internal

#endif

#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_ENCODER_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class BlockedHuffmanEncodeError : std::uint8_t {
    none,
    empty_input,
    block_too_large,
    model_output_too_small,
    payload_output_too_small,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct BlockedHuffmanEncodeResult {
    std::size_t model_size{};
    std::size_t payload_size{};
    BlockedHuffmanEncodeError error{BlockedHuffmanEncodeError::none};
};

[[nodiscard]] BlockedHuffmanEncodeResult plan_blocked_huffman_block(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    BlockedHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] BlockedHuffmanEncodeResult encode_blocked_huffman_block(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<std::byte> model_output,
    std::span<std::byte> payload_output,
    BlockedHuffmanDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_DECODER_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class BlockedHuffmanDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    model_size_mismatch,
    payload_size_mismatch,
    output_too_small,
    invalid_model,
    code_length_limit,
    decode_table_limit,
    nonzero_padding,
    truncated_payload,
    invalid_code,
    trailing_bits,
    internal_error,
};

struct BlockedHuffmanDecodeResult {
    std::size_t output_size{};
    BlockedHuffmanDecodeError error{BlockedHuffmanDecodeError::none};
};

[[nodiscard]] BlockedHuffmanDecodeResult decode_blocked_huffman_block(
    const BlockedHuffmanDescriptor& descriptor,
    std::span<const std::byte> model,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif

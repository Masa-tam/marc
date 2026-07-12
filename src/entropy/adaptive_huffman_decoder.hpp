#ifndef MARC_ENTROPY_ADAPTIVE_HUFFMAN_DECODER_HPP
#define MARC_ENTROPY_ADAPTIVE_HUFFMAN_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/adaptive_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class AdaptiveHuffmanDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    output_too_small,
    truncated_payload,
    duplicate_nyt_symbol,
    invalid_tree,
    trailing_bits,
    nonzero_padding,
    arithmetic_overflow,
    internal_error,
};

struct AdaptiveHuffmanDecodeResult {
    std::size_t output_size{};
    std::uint64_t bits_consumed{};
    AdaptiveHuffmanDecodeError error{AdaptiveHuffmanDecodeError::none};
};

[[nodiscard]] AdaptiveHuffmanDecodeResult decode_adaptive_huffman_frame(
    const AdaptiveHuffmanDescriptor& descriptor,
    std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif

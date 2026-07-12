#ifndef MARC_ENTROPY_ADAPTIVE_HUFFMAN_ENCODER_HPP
#define MARC_ENTROPY_ADAPTIVE_HUFFMAN_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/adaptive_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class AdaptiveHuffmanEncodeError : std::uint8_t {
    none,
    empty_input,
    frame_too_large,
    payload_output_too_small,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct AdaptiveHuffmanEncodeResult {
    std::size_t payload_size{};
    std::uint64_t payload_bits{};
    AdaptiveHuffmanEncodeError error{AdaptiveHuffmanEncodeError::none};
};

[[nodiscard]] AdaptiveHuffmanEncodeResult plan_adaptive_huffman_frame(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] AdaptiveHuffmanEncodeResult encode_adaptive_huffman_frame(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    AdaptiveHuffmanDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif

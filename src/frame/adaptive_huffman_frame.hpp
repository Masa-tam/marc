#ifndef MARC_FRAME_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_ADAPTIVE_HUFFMAN_FRAME_HPP

#include "core/limits.hpp"
#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class AdaptiveHuffmanFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    descriptor_error,
    body_encode_error,
    body_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct AdaptiveHuffmanFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::AdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::AdaptiveHuffmanFormatError::none};
    entropy::internal::AdaptiveHuffmanEncodeError encode_error{
        entropy::internal::AdaptiveHuffmanEncodeError::none};
    entropy::internal::AdaptiveHuffmanDecodeError decode_error{
        entropy::internal::AdaptiveHuffmanDecodeError::none};
    AdaptiveHuffmanFrameCodecError error{AdaptiveHuffmanFrameCodecError::none};
};

[[nodiscard]] AdaptiveHuffmanFrameCodecResult plan_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] AdaptiveHuffmanFrameCodecResult encode_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] AdaptiveHuffmanFrameCodecResult decode_adaptive_huffman_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_BLOCKED_HUFFMAN_FRAME_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class BlockedHuffmanFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    body_encode_error,
    view_output_too_small,
    controller_error,
    body_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct BlockedHuffmanFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t block_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::BlockedHuffmanFrameEncodeError encode_error{
        entropy::internal::BlockedHuffmanFrameEncodeError::none};
    entropy::internal::BlockedHuffmanControllerError controller_error{
        entropy::internal::BlockedHuffmanControllerError::none};
    entropy::internal::BlockedHuffmanFrameDecodeError decode_error{
        entropy::internal::BlockedHuffmanFrameDecodeError::none};
    BlockedHuffmanFrameCodecError error{
        BlockedHuffmanFrameCodecError::none};
};

[[nodiscard]] BlockedHuffmanFrameCodecResult plan_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] BlockedHuffmanFrameCodecResult encode_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> output) noexcept;

[[nodiscard]] BlockedHuffmanFrameCodecResult decode_blocked_huffman_frame(
    const StreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

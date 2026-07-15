#ifndef MARC_FRAME_LZ77_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZ77_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lz77_decoder.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77BlockedHuffmanFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    view_output_too_small,
    dictionary_staging_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    raw_output_too_small,
    dictionary_decode_error,
    arithmetic_overflow,
};

struct Lz77BlockedHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t block_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::BlockedHuffmanControllerError controller_error{
        entropy::internal::BlockedHuffmanControllerError::none};
    entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
        entropy::internal::BlockedHuffmanFrameDecodeError::none};
    dictionary::internal::Lz77ValidationError dictionary_error{
        dictionary::internal::Lz77ValidationError::none};
    dictionary::internal::Lz77FormatError dictionary_format_error{
        dictionary::internal::Lz77FormatError::none};
    dictionary::internal::Lz77DecodeError dictionary_decode_error{
        dictionary::internal::Lz77DecodeError::none};
    Lz77BlockedHuffmanFrameValidationError error{
        Lz77BlockedHuffmanFrameValidationError::none};
};

// Validates and entropy-decodes one complete frame into dictionary_staging.
// Input and staging must not overlap. This function deliberately does not
// expose or write decoded raw bytes.
[[nodiscard]] Lz77BlockedHuffmanFrameValidationResult
validate_lz77_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates the complete frame before publishing any raw byte. The serialized
// input, dictionary staging, and raw output extents must be mutually
// non-overlapping.
[[nodiscard]] Lz77BlockedHuffmanFrameValidationResult
decode_lz77_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

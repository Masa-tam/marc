#ifndef MARC_FRAME_LZSS_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZSS_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lzss_decoder.hpp"
#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_validator.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssBlockedHuffmanFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    serialized_output_too_small,
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
    dictionary_encode_error,
    entropy_encode_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssBlockedHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t block_count{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::BlockedHuffmanControllerError controller_error{
        entropy::internal::BlockedHuffmanControllerError::none};
    entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
        entropy::internal::BlockedHuffmanFrameDecodeError::none};
    dictionary::internal::LzssValidationError dictionary_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError dictionary_format_error{
        dictionary::internal::LzssFormatError::none};
    dictionary::internal::LzssDecodeError dictionary_decode_error{
        dictionary::internal::LzssDecodeError::none};
    dictionary::internal::LzssEncodeError dictionary_encode_error{
        dictionary::internal::LzssEncodeError::none};
    entropy::internal::BlockedHuffmanFrameEncodeError entropy_encode_error{
        entropy::internal::BlockedHuffmanFrameEncodeError::none};
    LzssBlockedHuffmanFrameValidationError error{
        LzssBlockedHuffmanFrameValidationError::none};
};

// Produces the canonical LZSS staging needed to determine the exact Blocked
// Huffman representation. Input and staging must not overlap.
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult
plan_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, dictionary
// staging, and serialized output must be mutually non-overlapping.
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult
encode_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Validates and entropy-decodes one complete frame into dictionary_staging.
// Input and staging must not overlap. Raw bytes are deliberately not exposed.
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult
validate_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates the complete frame before publishing any raw byte. The serialized
// input, dictionary staging, and raw output extents must be mutually
// non-overlapping.
[[nodiscard]] LzssBlockedHuffmanFrameValidationResult
decode_lzss_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

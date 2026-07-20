#ifndef MARC_FRAME_LZW_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZW_ADAPTIVE_HUFFMAN_FRAME_HPP

#include "dictionary/lzw_decoder.hpp"
#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwAdaptiveHuffmanFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    workspace_limit,
    descriptor_error,
    entropy_decode_error,
    dictionary_validation_error,
    arithmetic_overflow,
    raw_staging_too_small,
    dictionary_decode_error,
    raw_output_too_small,
};

struct LzwAdaptiveHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t phrase_entries{};
    std::size_t code_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::AdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::AdaptiveHuffmanFormatError::none};
    entropy::internal::AdaptiveHuffmanDecodeError entropy_error{
        entropy::internal::AdaptiveHuffmanDecodeError::none};
    dictionary::internal::LzwValidationError dictionary_error{
        dictionary::internal::LzwValidationError::none};
    dictionary::internal::LzwFormatError dictionary_format_error{
        dictionary::internal::LzwFormatError::none};
    dictionary::internal::LzwDecodeError dictionary_decode_error{
        dictionary::internal::LzwDecodeError::none};
    LzwAdaptiveHuffmanFrameValidationError error{
        LzwAdaptiveHuffmanFrameValidationError::none};
};

// Entropy-decodes and validates exactly one complete frame into private packed
// LZW staging and a caller-owned phrase table. No raw byte is reconstructed or
// published. On error, the caller must discard both workspace contents. Input
// and dictionary staging must not overlap.
[[nodiscard]] LzwAdaptiveHuffmanFrameValidationResult
validate_lzw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry>
        phrase_workspace) noexcept;

// Validates every encoded layer and reconstructs exactly one frame into
// caller-owned private raw staging. On error, all workspace contents must be
// discarded. Input, dictionary staging, and raw staging must not overlap.
[[nodiscard]] LzwAdaptiveHuffmanFrameValidationResult
decode_lzw_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then copies the complete raw frame to
// caller-visible output only after every operation succeeds. Input, dictionary
// staging, raw staging, and output must be mutually non-overlapping.
[[nodiscard]] LzwAdaptiveHuffmanFrameValidationResult
decode_lzw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

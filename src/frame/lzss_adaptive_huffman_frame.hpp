#ifndef MARC_FRAME_LZSS_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZSS_ADAPTIVE_HUFFMAN_FRAME_HPP

#include "dictionary/lzss_decoder.hpp"
#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_validator.hpp"
#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssAdaptiveHuffmanFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    serialized_output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    dictionary_staging_too_small,
    raw_staging_too_small,
    raw_output_too_small,
    workspace_limit,
    descriptor_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    dictionary_encode_error,
    entropy_encode_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssAdaptiveHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::AdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::AdaptiveHuffmanFormatError::none};
    entropy::internal::AdaptiveHuffmanDecodeError entropy_error{
        entropy::internal::AdaptiveHuffmanDecodeError::none};
    dictionary::internal::LzssValidationError dictionary_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError dictionary_format_error{
        dictionary::internal::LzssFormatError::none};
    dictionary::internal::LzssDecodeError dictionary_decode_error{
        dictionary::internal::LzssDecodeError::none};
    dictionary::internal::LzssEncodeError dictionary_encode_error{
        dictionary::internal::LzssEncodeError::none};
    entropy::internal::AdaptiveHuffmanEncodeError entropy_encode_error{
        entropy::internal::AdaptiveHuffmanEncodeError::none};
    LzssAdaptiveHuffmanFrameValidationError error{
        LzssAdaptiveHuffmanFrameValidationError::none};
};

// Produces canonical LZSS staging and determines the complete frame extent.
// Input and staging must not overlap.
[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult
plan_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, staging, and
// output must be mutually non-overlapping.
[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult
encode_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Validates and entropy-decodes one exact frame into private canonical LZSS
// token staging. No raw byte is reconstructed or published. Input and staging
// must not overlap.
[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult
validate_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates every layer and reconstructs exactly one frame into private raw
// staging without publishing it to caller-visible output.
[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult
decode_lzss_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging) noexcept;

// Validates every layer, reconstructs into private raw staging, and publishes
// only after reconstruction succeeds. Input, both staging extents, and output
// must be mutually non-overlapping.
[[nodiscard]] LzssAdaptiveHuffmanFrameValidationResult
decode_lzss_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

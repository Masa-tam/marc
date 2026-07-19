#ifndef MARC_FRAME_LZ78_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZ78_ADAPTIVE_HUFFMAN_FRAME_HPP

#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78AdaptiveHuffmanFrameValidationError : std::uint8_t {
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
    encoder_workspace_too_small,
    phrase_workspace_too_small,
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

struct Lz78AdaptiveHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t phrase_entries{};
    std::size_t encoder_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::AdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::AdaptiveHuffmanFormatError::none};
    entropy::internal::AdaptiveHuffmanDecodeError entropy_error{
        entropy::internal::AdaptiveHuffmanDecodeError::none};
    dictionary::internal::Lz78ValidationError dictionary_error{
        dictionary::internal::Lz78ValidationError::none};
    dictionary::internal::Lz78FormatError dictionary_format_error{
        dictionary::internal::Lz78FormatError::none};
    dictionary::internal::Lz78DecodeError dictionary_decode_error{
        dictionary::internal::Lz78DecodeError::none};
    dictionary::internal::Lz78EncodeError dictionary_encode_error{
        dictionary::internal::Lz78EncodeError::none};
    entropy::internal::AdaptiveHuffmanEncodeError entropy_encode_error{
        entropy::internal::AdaptiveHuffmanEncodeError::none};
    Lz78AdaptiveHuffmanFrameValidationError error{
        Lz78AdaptiveHuffmanFrameValidationError::none};
};

// Fixes the complete LZ78 parse in private token staging, then plans Adaptive
// Huffman over those exact bytes. Raw input and staging must not overlap.
[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult
plan_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, token staging,
// and serialized output must be mutually non-overlapping.
[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult
encode_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Entropy-decodes and validates exactly one frame into private canonical LZ78
// token staging and a caller-owned phrase table. No raw byte is reconstructed
// or published. On error, the caller must discard both workspace contents.
// Input and token staging must not overlap.
[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult
validate_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace) noexcept;

// Validates every encoded layer and reconstructs exactly one frame into
// private raw staging. On error, all three workspace contents must be
// discarded. Input, token staging, and raw staging must not overlap.
[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult
decode_lz78_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then copies the complete raw frame to
// caller-visible output only after every operation succeeds. Input, token
// staging, raw staging, and output must be mutually non-overlapping.
[[nodiscard]] Lz78AdaptiveHuffmanFrameValidationResult
decode_lz78_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

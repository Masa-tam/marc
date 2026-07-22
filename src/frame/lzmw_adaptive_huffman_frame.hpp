#ifndef MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_FRAME_HPP

#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_encoder.hpp"
#include "entropy/adaptive_huffman_decoder.hpp"
#include "entropy/adaptive_huffman_encoder.hpp"
#include "entropy/adaptive_huffman_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzmwAdaptiveHuffmanFrameValidationError : std::uint8_t {
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
    expansion_workspace_too_small,
    dictionary_decode_error,
    raw_output_too_small,
    input_size_mismatch,
    serialized_output_too_small,
    encoder_workspace_too_small,
    dictionary_encode_error,
    entropy_encode_error,
    internal_error,
};

struct LzmwAdaptiveHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
    std::size_t token_count{};
    std::size_t dictionary_entries{};
    std::size_t encoder_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::AdaptiveHuffmanFormatError descriptor_error{
        entropy::internal::AdaptiveHuffmanFormatError::none};
    entropy::internal::AdaptiveHuffmanDecodeError entropy_error{
        entropy::internal::AdaptiveHuffmanDecodeError::none};
    dictionary::internal::LzmwValidationError dictionary_error{
        dictionary::internal::LzmwValidationError::none};
    dictionary::internal::LzmwFormatError dictionary_format_error{
        dictionary::internal::LzmwFormatError::none};
    dictionary::internal::LzmwDecodeError dictionary_decode_error{
        dictionary::internal::LzmwDecodeError::none};
    dictionary::internal::LzmwEncodeError dictionary_encode_error{
        dictionary::internal::LzmwEncodeError::none};
    entropy::internal::AdaptiveHuffmanEncodeError entropy_encode_error{
        entropy::internal::AdaptiveHuffmanEncodeError::none};
    LzmwAdaptiveHuffmanFrameValidationError error{
        LzmwAdaptiveHuffmanFrameValidationError::none};
};

// Fixes the complete canonical LZMW reference stream in private dictionary
// staging before planning Adaptive Huffman over those exact bytes. Raw input
// and staging must not overlap.
[[nodiscard]] LzmwAdaptiveHuffmanFrameValidationResult
plan_lzmw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, dictionary
// staging, and serialized output must be mutually non-overlapping.
[[nodiscard]] LzmwAdaptiveHuffmanFrameValidationResult
encode_lzmw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Entropy-decodes and validates exactly one complete frame into private LZMW
// reference staging and a caller-owned phrase table. No raw byte is
// reconstructed or published. On error, the caller must discard both
// workspace contents. Input and dictionary staging must not overlap.
[[nodiscard]] LzmwAdaptiveHuffmanFrameValidationResult
validate_lzmw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept;

// Validates every encoded layer and reconstructs exactly one frame into
// caller-owned private raw staging. On error, all workspace contents must be
// discarded. Input, dictionary staging, and raw staging must not overlap.
[[nodiscard]] LzmwAdaptiveHuffmanFrameValidationResult
decode_lzmw_adaptive_huffman_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then copies the complete raw frame to
// caller-visible output only after every operation succeeds. All supplied
// storage regions must be mutually non-overlapping.
[[nodiscard]] LzmwAdaptiveHuffmanFrameValidationResult
decode_lzmw_adaptive_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

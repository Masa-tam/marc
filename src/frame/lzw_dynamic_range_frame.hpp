#ifndef MARC_FRAME_LZW_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZW_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_encoder.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwDynamicRangeFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    raw_staging_too_small,
    workspace_limit,
    descriptor_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    arithmetic_overflow,
    raw_output_too_small,
    input_size_mismatch,
    encoder_workspace_too_small,
    dictionary_encode_error,
    entropy_encode_error,
    internal_error,
};

struct LzwDynamicRangeFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t encoder_entries{};
    std::size_t phrase_entries{};
    std::size_t code_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeDecodeError entropy_error{
        entropy::internal::DynamicRangeDecodeError::none};
    dictionary::internal::LzwValidationError dictionary_error{
        dictionary::internal::LzwValidationError::none};
    dictionary::internal::LzwFormatError dictionary_format_error{
        dictionary::internal::LzwFormatError::none};
    dictionary::internal::LzwDecodeError dictionary_decode_error{
        dictionary::internal::LzwDecodeError::none};
    dictionary::internal::LzwEncodeError dictionary_encode_error{
        dictionary::internal::LzwEncodeError::none};
    entropy::internal::DynamicRangeEncodeError entropy_encode_error{
        entropy::internal::DynamicRangeEncodeError::none};
    LzwDynamicRangeFrameValidationError error{
        LzwDynamicRangeFrameValidationError::none};
};

// Fixes the complete packed LZW code stream, including final zero padding,
// before planning Dynamic Range over those exact bytes. It reports the exact
// complete-frame extent without writing serialized output. Raw input and
// packed staging must not overlap.
[[nodiscard]] LzwDynamicRangeFrameValidationResult
plan_lzw_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Entropy-decodes and validates exactly one complete frame into private packed
// LZW staging and a caller-owned phrase table. No raw byte is reconstructed or
// published. On error, the caller must discard both workspace contents. Input
// and dictionary staging must not overlap.
[[nodiscard]] LzwDynamicRangeFrameValidationResult
validate_lzw_dynamic_range_frame(
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
[[nodiscard]] LzwDynamicRangeFrameValidationResult
decode_lzw_dynamic_range_frame_to_staging(
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
[[nodiscard]] LzwDynamicRangeFrameValidationResult
decode_lzw_dynamic_range_frame(
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

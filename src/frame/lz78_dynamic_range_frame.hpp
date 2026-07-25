#ifndef MARC_FRAME_LZ78_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZ78_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_format.hpp"
#include "dictionary/lz78_validator.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78DynamicRangeFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    encoder_workspace_too_small,
    dictionary_staging_too_small,
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
};

struct Lz78DynamicRangeFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t encoder_entries{};
    std::size_t phrase_entries{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeDecodeError entropy_error{
        entropy::internal::DynamicRangeDecodeError::none};
    dictionary::internal::Lz78ValidationError dictionary_error{
        dictionary::internal::Lz78ValidationError::none};
    dictionary::internal::Lz78FormatError dictionary_format_error{
        dictionary::internal::Lz78FormatError::none};
    dictionary::internal::Lz78DecodeError dictionary_decode_error{
        dictionary::internal::Lz78DecodeError::none};
    dictionary::internal::Lz78EncodeError dictionary_encode_error{
        dictionary::internal::Lz78EncodeError::none};
    entropy::internal::DynamicRangeEncodeError entropy_encode_error{
        entropy::internal::DynamicRangeEncodeError::none};
    Lz78DynamicRangeFrameValidationError error{
        Lz78DynamicRangeFrameValidationError::none};
};

// Produces canonical LZ78 token staging and determines the complete frame
// extent without writing a serialized frame. Input and token staging must not
// overlap.
[[nodiscard]] Lz78DynamicRangeFrameValidationResult
plan_lz78_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Entropy-decodes and validates exactly one frame into private canonical LZ78
// token staging and a caller-owned phrase table. No raw byte is reconstructed
// or published. On error, the caller must discard both workspace contents.
// Input and token staging must not overlap.
[[nodiscard]] Lz78DynamicRangeFrameValidationResult
validate_lz78_dynamic_range_frame(
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
[[nodiscard]] Lz78DynamicRangeFrameValidationResult
decode_lz78_dynamic_range_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates every layer, reconstructs into private raw staging, and publishes
// only after reconstruction succeeds. Input, token staging, phrase workspace,
// raw staging, and output must be mutually non-overlapping.
[[nodiscard]] Lz78DynamicRangeFrameValidationResult
decode_lz78_dynamic_range_frame(
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

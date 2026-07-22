#ifndef MARC_FRAME_LZ77_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZ77_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lz77_decoder.hpp"
#include "dictionary/lz77_encoder.hpp"
#include "dictionary/lz77_format.hpp"
#include "dictionary/lz77_validator.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77DynamicRangeFrameValidationError : std::uint8_t {
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

struct Lz77DynamicRangeFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeDecodeError entropy_error{
        entropy::internal::DynamicRangeDecodeError::none};
    dictionary::internal::Lz77ValidationError dictionary_error{
        dictionary::internal::Lz77ValidationError::none};
    dictionary::internal::Lz77FormatError dictionary_format_error{
        dictionary::internal::Lz77FormatError::none};
    dictionary::internal::Lz77DecodeError dictionary_decode_error{
        dictionary::internal::Lz77DecodeError::none};
    dictionary::internal::Lz77EncodeError dictionary_encode_error{
        dictionary::internal::Lz77EncodeError::none};
    entropy::internal::DynamicRangeEncodeError entropy_encode_error{
        entropy::internal::DynamicRangeEncodeError::none};
    Lz77DynamicRangeFrameValidationError error{
        Lz77DynamicRangeFrameValidationError::none};
};

// Produces canonical LZ77 staging and determines the complete frame extent.
// Input and staging must not overlap.
[[nodiscard]] Lz77DynamicRangeFrameValidationResult
plan_lz77_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, staging, and
// output must be mutually non-overlapping.
[[nodiscard]] Lz77DynamicRangeFrameValidationResult
encode_lz77_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Validates and entropy-decodes one complete frame into private canonical
// LZ77-token staging. No raw byte is reconstructed or published here. Input
// and staging must not overlap.
[[nodiscard]] Lz77DynamicRangeFrameValidationResult
validate_lz77_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates every layer and reconstructs exactly one frame into private raw
// staging without publishing it to a caller-visible output extent.
[[nodiscard]] Lz77DynamicRangeFrameValidationResult
decode_lz77_dynamic_range_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging) noexcept;

// Validates every layer, reconstructs into private raw staging, and publishes
// to output only after reconstruction succeeds. Input, both staging extents,
// and output must be mutually non-overlapping.
[[nodiscard]] Lz77DynamicRangeFrameValidationResult
decode_lz77_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

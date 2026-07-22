#ifndef MARC_FRAME_LZ77_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZ77_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lz77_format.hpp"
#include "dictionary/lz77_validator.hpp"
#include "entropy/dynamic_range_decoder.hpp"
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
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    dictionary_staging_too_small,
    workspace_limit,
    descriptor_error,
    entropy_decode_error,
    dictionary_validation_error,
    arithmetic_overflow,
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
    Lz77DynamicRangeFrameValidationError error{
        Lz77DynamicRangeFrameValidationError::none};
};

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

} // namespace marc::frame

#endif

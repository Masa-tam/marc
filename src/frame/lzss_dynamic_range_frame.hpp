#ifndef MARC_FRAME_LZSS_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZSS_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_validator.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssDynamicRangeFrameValidationError : std::uint8_t {
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

struct LzssDynamicRangeFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeDecodeError entropy_error{
        entropy::internal::DynamicRangeDecodeError::none};
    dictionary::internal::LzssValidationError dictionary_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError dictionary_format_error{
        dictionary::internal::LzssFormatError::none};
    LzssDynamicRangeFrameValidationError error{
        LzssDynamicRangeFrameValidationError::none};
};

// Validates and entropy-decodes one exact frame into private canonical LZSS
// token staging. No raw byte is reconstructed or published. Input and staging
// must not overlap.
[[nodiscard]] LzssDynamicRangeFrameValidationResult
validate_lzss_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZ77_RANS_FRAME_HPP
#define MARC_FRAME_LZ77_RANS_FRAME_HPP

#include "dictionary/lz77_decoder.hpp"
#include "dictionary/lz77_format.hpp"
#include "dictionary/lz77_validator.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77RansFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    views_too_small,
    dictionary_staging_too_small,
    raw_staging_too_small,
    raw_output_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct Lz77RansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansDecodeError entropy_error{
        entropy::internal::RansDecodeError::none};
    dictionary::internal::Lz77ValidationError dictionary_error{
        dictionary::internal::Lz77ValidationError::none};
    dictionary::internal::Lz77FormatError dictionary_format_error{
        dictionary::internal::Lz77FormatError::none};
    dictionary::internal::Lz77DecodeError dictionary_decode_error{
        dictionary::internal::Lz77DecodeError::none};
    Lz77RansFrameValidationError error{
        Lz77RansFrameValidationError::none};
};

// Validates and rANS-decodes one complete frame into private canonical
// LZ77-token staging. All rANS blocks are validated before any token byte is
// written. No raw byte is reconstructed or published here. Input, views, and
// staging must not overlap.
[[nodiscard]] Lz77RansFrameValidationResult validate_lz77_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates every layer and reconstructs exactly one frame into private raw
// staging without publishing it to a caller-visible output extent.
[[nodiscard]] Lz77RansFrameValidationResult
decode_lz77_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging) noexcept;

// Validates every layer, reconstructs into private raw staging, and publishes
// to output only after reconstruction succeeds. Input, views, both staging
// extents, and output must be mutually non-overlapping.
[[nodiscard]] Lz77RansFrameValidationResult decode_lz77_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

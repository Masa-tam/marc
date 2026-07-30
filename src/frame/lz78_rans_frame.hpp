#ifndef MARC_FRAME_LZ78_RANS_FRAME_HPP
#define MARC_FRAME_LZ78_RANS_FRAME_HPP

#include "dictionary/lz78_format.hpp"
#include "dictionary/lz78_validator.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78RansFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    views_too_small,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    arithmetic_overflow,
    internal_error,
};

struct Lz78RansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t phrase_entries{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansDecodeError entropy_error{
        entropy::internal::RansDecodeError::none};
    dictionary::internal::Lz78ValidationError dictionary_error{
        dictionary::internal::Lz78ValidationError::none};
    dictionary::internal::Lz78FormatError dictionary_format_error{
        dictionary::internal::Lz78FormatError::none};
    Lz78RansFrameValidationError error{
        Lz78RansFrameValidationError::none};
};

// Validates every rANS block before reconstructing the private canonical LZ78
// token region, then validates the complete phrase graph without expanding or
// publishing raw bytes. All caller-owned regions must be non-overlapping.
[[nodiscard]] Lz78RansFrameValidationResult validate_lz78_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace) noexcept;

} // namespace marc::frame

#endif

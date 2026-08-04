#ifndef MARC_FRAME_LZD_TANS_FRAME_HPP
#define MARC_FRAME_LZD_TANS_FRAME_HPP

#include "dictionary/lzd_format.hpp"
#include "dictionary/lzd_validator.hpp"
#include "entropy/tans_controller.hpp"
#include "entropy/tans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzdTansFrameValidationError : std::uint8_t {
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

struct LzdTansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t phrase_entries{};
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t dictionary_input_offset{};
    std::uint32_t dictionary_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::TansControllerError controller_error{
        entropy::internal::TansControllerError::none};
    entropy::internal::TansDecodeError entropy_error{
        entropy::internal::TansDecodeError::none};
    dictionary::internal::LzdValidationError dictionary_error{
        dictionary::internal::LzdValidationError::none};
    dictionary::internal::LzdFormatError dictionary_format_error{
        dictionary::internal::LzdFormatError::none};
    LzdTansFrameValidationError error{LzdTansFrameValidationError::none};
};

// Validates every serialized tANS block before reconstructing the private
// canonical LZD token region, then validates the complete phrase graph without
// expanding or publishing raw bytes. All caller-owned regions must be
// mutually non-overlapping and discarded after any error.
[[nodiscard]] LzdTansFrameValidationResult validate_lzd_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace) noexcept;

} // namespace marc::frame

#endif

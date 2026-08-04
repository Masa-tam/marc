#ifndef MARC_FRAME_LZW_TANS_FRAME_HPP
#define MARC_FRAME_LZW_TANS_FRAME_HPP

#include "dictionary/lzw_format.hpp"
#include "dictionary/lzw_validator.hpp"
#include "entropy/tans_controller.hpp"
#include "entropy/tans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwTansFrameValidationError : std::uint8_t {
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

struct LzwTansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t phrase_entries{};
    std::size_t code_count{};
    std::size_t code_index{};
    std::size_t dictionary_input_offset{};
    std::uint64_t dictionary_input_bit_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::TansControllerError controller_error{
        entropy::internal::TansControllerError::none};
    entropy::internal::TansDecodeError entropy_error{
        entropy::internal::TansDecodeError::none};
    dictionary::internal::LzwValidationError dictionary_error{
        dictionary::internal::LzwValidationError::none};
    dictionary::internal::LzwFormatError dictionary_format_error{
        dictionary::internal::LzwFormatError::none};
    LzwTansFrameValidationError error{LzwTansFrameValidationError::none};
};

// Validates every tANS block before reconstructing the private packed LZW
// byte region, then validates the complete code graph without expanding or
// publishing raw bytes. All caller-owned regions must be non-overlapping.
[[nodiscard]] LzwTansFrameValidationResult validate_lzw_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry> phrase_workspace) noexcept;

} // namespace marc::frame

#endif

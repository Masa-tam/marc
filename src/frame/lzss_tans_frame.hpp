#ifndef MARC_FRAME_LZSS_TANS_FRAME_HPP
#define MARC_FRAME_LZSS_TANS_FRAME_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_validator.hpp"
#include "entropy/tans_controller.hpp"
#include "entropy/tans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssTansFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    views_too_small,
    dictionary_staging_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssTansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::TansControllerError controller_error{
        entropy::internal::TansControllerError::none};
    entropy::internal::TansDecodeError entropy_error{
        entropy::internal::TansDecodeError::none};
    dictionary::internal::LzssValidationError dictionary_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError dictionary_format_error{
        dictionary::internal::LzssFormatError::none};
    LzssTansFrameValidationError error{
        LzssTansFrameValidationError::none};
};

// Validates and tANS-decodes one complete frame into private canonical LZSS
// token staging. Every tANS block is validated before any token byte is
// written. The complete variable-length LZSS grammar is then validated
// without reconstructing or publishing raw bytes. Input, views, and staging
// must not overlap.
[[nodiscard]] LzssTansFrameValidationResult validate_lzss_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views,
    std::span<std::byte> dictionary_staging) noexcept;

} // namespace marc::frame

#endif

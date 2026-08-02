#ifndef MARC_FRAME_LZMW_RANS_FRAME_HPP
#define MARC_FRAME_LZMW_RANS_FRAME_HPP

#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_format.hpp"
#include "dictionary/lzmw_validator.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzmwRansFrameValidationError : std::uint8_t {
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
    expansion_workspace_too_small,
    raw_staging_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct LzmwRansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t dictionary_input_offset{};
    std::uint32_t dictionary_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansDecodeError entropy_error{
        entropy::internal::RansDecodeError::none};
    dictionary::internal::LzmwValidationError dictionary_error{
        dictionary::internal::LzmwValidationError::none};
    dictionary::internal::LzmwFormatError dictionary_format_error{
        dictionary::internal::LzmwFormatError::none};
    dictionary::internal::LzmwDecodeError dictionary_decode_error{
        dictionary::internal::LzmwDecodeError::none};
    LzmwRansFrameValidationError error{LzmwRansFrameValidationError::none};
};

// Validates every rANS block before reconstructing the private canonical LZMW
// reference region, then validates the complete phrase graph without expanding
// or publishing raw bytes. All caller-owned regions must be non-overlapping.
[[nodiscard]] LzmwRansFrameValidationResult validate_lzmw_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept;

// Validates every encoded layer and reconstructs exactly one frame into
// caller-owned private raw staging. On error, all workspace contents must be
// discarded. Input, reference staging, and raw staging must not overlap.
[[nodiscard]] LzmwRansFrameValidationResult
decode_lzmw_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZMW_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_LZMW_DYNAMIC_RANGE_FRAME_HPP

#include "dictionary/lzmw_decoder.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzmwDynamicRangeFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    raw_staging_too_small,
    expansion_workspace_too_small,
    raw_output_too_small,
    workspace_limit,
    descriptor_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    arithmetic_overflow,
};

struct LzmwDynamicRangeFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
    std::size_t token_count{};
    std::size_t dictionary_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeDecodeError entropy_error{
        entropy::internal::DynamicRangeDecodeError::none};
    dictionary::internal::LzmwValidationError dictionary_error{
        dictionary::internal::LzmwValidationError::none};
    dictionary::internal::LzmwFormatError dictionary_format_error{
        dictionary::internal::LzmwFormatError::none};
    dictionary::internal::LzmwDecodeError dictionary_decode_error{
        dictionary::internal::LzmwDecodeError::none};
    LzmwDynamicRangeFrameValidationError error{
        LzmwDynamicRangeFrameValidationError::none};
};

// Entropy-decodes and validates exactly one complete frame into private LZMW
// reference staging and a caller-owned phrase table. No raw byte is
// reconstructed or published. On error, the caller must discard both
// workspace contents. Input and dictionary staging must not overlap.
[[nodiscard]] LzmwDynamicRangeFrameValidationResult
validate_lzmw_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept;

// Validates every encoded layer and reconstructs exactly one frame into
// caller-owned private raw staging. On error, all workspace contents must be
// discarded. Input, dictionary staging, and raw staging must not overlap.
[[nodiscard]] LzmwDynamicRangeFrameValidationResult
decode_lzmw_dynamic_range_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then copies the complete raw frame to
// caller-visible output only after every operation succeeds. All supplied
// storage regions must be mutually non-overlapping.
[[nodiscard]] LzmwDynamicRangeFrameValidationResult
decode_lzmw_dynamic_range_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

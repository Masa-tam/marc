#ifndef MARC_FRAME_LZD_TANS_FRAME_HPP
#define MARC_FRAME_LZD_TANS_FRAME_HPP

#include "dictionary/lzd_decoder.hpp"
#include "dictionary/lzd_encoder.hpp"
#include "dictionary/lzd_format.hpp"
#include "dictionary/lzd_validator.hpp"
#include "entropy/tans_controller.hpp"
#include "entropy/tans_decoder.hpp"
#include "entropy/tans_encoder.hpp"
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
    expansion_workspace_too_small,
    raw_staging_too_small,
    raw_output_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    arithmetic_overflow,
    internal_error,
    input_size_mismatch,
    encoder_workspace_too_small,
    dictionary_encode_error,
    entropy_encode_error,
};

struct LzdTansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t encoder_entries{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
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
    dictionary::internal::LzdDecodeError dictionary_decode_error{
        dictionary::internal::LzdDecodeError::none};
    dictionary::internal::LzdEncodeError dictionary_encode_error{
        dictionary::internal::LzdEncodeError::none};
    entropy::internal::TansEncodeError entropy_encode_error{
        entropy::internal::TansEncodeError::none};
    LzdTansFrameValidationError error{LzdTansFrameValidationError::none};
};

// Fixes the complete canonical LZD token stream before planning every tANS
// block and reports the exact complete-frame extent without writing serialized
// output. Input, encoder workspace, and token staging must not overlap.
[[nodiscard]] LzdTansFrameValidationResult plan_lzd_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzdEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

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

// Validates every encoded layer and reconstructs exactly one frame into
// caller-owned private raw staging. On error, all workspace contents must be
// discarded. Input, token staging, and raw staging must not overlap.
[[nodiscard]] LzdTansFrameValidationResult
decode_lzd_tans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then publishes exactly the declared
// raw extent only after every layer succeeds. All caller-owned regions must be
// mutually non-overlapping.
[[nodiscard]] LzdTansFrameValidationResult decode_lzd_tans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

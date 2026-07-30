#ifndef MARC_FRAME_LZ78_RANS_FRAME_HPP
#define MARC_FRAME_LZ78_RANS_FRAME_HPP

#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_format.hpp"
#include "dictionary/lz78_validator.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "entropy/rans_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78RansFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    serialized_output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    encoder_workspace_too_small,
    views_too_small,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    raw_staging_too_small,
    raw_output_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    dictionary_encode_error,
    entropy_encode_error,
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
    std::size_t encoder_entries{};
    std::size_t phrase_entries{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansDecodeError entropy_error{
        entropy::internal::RansDecodeError::none};
    entropy::internal::RansFormatError descriptor_error{
        entropy::internal::RansFormatError::none};
    entropy::internal::RansEncodeError entropy_encode_error{
        entropy::internal::RansEncodeError::none};
    dictionary::internal::Lz78ValidationError dictionary_error{
        dictionary::internal::Lz78ValidationError::none};
    dictionary::internal::Lz78FormatError dictionary_format_error{
        dictionary::internal::Lz78FormatError::none};
    dictionary::internal::Lz78DecodeError dictionary_decode_error{
        dictionary::internal::Lz78DecodeError::none};
    dictionary::internal::Lz78EncodeError dictionary_encode_error{
        dictionary::internal::Lz78EncodeError::none};
    Lz78RansFrameValidationError error{
        Lz78RansFrameValidationError::none};
};

// Produces canonical LZ78 token staging and determines every rANS block and
// the complete frame extent without writing serialized output. Input, encoder
// workspace, and token staging must be mutually non-overlapping.
[[nodiscard]] Lz78RansFrameValidationResult plan_lz78_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing the generic header, rANS descriptors, or
// payloads. All caller-owned regions must be mutually non-overlapping.
[[nodiscard]] Lz78RansFrameValidationResult encode_lz78_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

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

// Validates every encoded layer and reconstructs exactly one frame into
// private raw staging. On error, all workspaces must be discarded. Input,
// token staging, and raw staging must be mutually non-overlapping.
[[nodiscard]] Lz78RansFrameValidationResult
decode_lz78_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging) noexcept;

// Validates and reconstructs privately, then publishes exactly the declared
// raw extent only after every layer succeeds. All caller-owned regions must
// be mutually non-overlapping.
[[nodiscard]] Lz78RansFrameValidationResult decode_lz78_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

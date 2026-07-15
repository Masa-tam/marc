#ifndef MARC_FRAME_LZMW_FRAME_HPP
#define MARC_FRAME_LZMW_FRAME_HPP

#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzmwFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    body_encode_error,
    body_decode_error,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct LzmwFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::LzmwEncodeError encode_error{
        dictionary::internal::LzmwEncodeError::none};
    dictionary::internal::LzmwDecodeError decode_error{
        dictionary::internal::LzmwDecodeError::none};
    dictionary::internal::LzmwValidationError validation_error{
        dictionary::internal::LzmwValidationError::none};
    dictionary::internal::LzmwFormatError format_error{
        dictionary::internal::LzmwFormatError::none};
    LzmwFrameCodecError error{LzmwFrameCodecError::none};
};

[[nodiscard]] LzmwFrameCodecResult plan_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzmwFrameCodecResult encode_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzmwFrameCodecResult validate_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept;

[[nodiscard]] LzmwFrameCodecResult decode_lzmw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

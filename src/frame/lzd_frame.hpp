#ifndef MARC_FRAME_LZD_FRAME_HPP
#define MARC_FRAME_LZD_FRAME_HPP

#include "dictionary/lzd_decoder.hpp"
#include "dictionary/lzd_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzdFrameCodecError : std::uint8_t {
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

struct LzdFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::LzdEncodeError encode_error{
        dictionary::internal::LzdEncodeError::none};
    dictionary::internal::LzdDecodeError decode_error{
        dictionary::internal::LzdDecodeError::none};
    dictionary::internal::LzdValidationError validation_error{
        dictionary::internal::LzdValidationError::none};
    dictionary::internal::LzdFormatError format_error{
        dictionary::internal::LzdFormatError::none};
    LzdFrameCodecError error{LzdFrameCodecError::none};
};

[[nodiscard]] LzdFrameCodecResult plan_lzd_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzdEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzdFrameCodecResult encode_lzd_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzdEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzdFrameCodecResult validate_lzd_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzdPhraseEntry>
        phrase_workspace) noexcept;

[[nodiscard]] LzdFrameCodecResult decode_lzd_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZW_FRAME_HPP
#define MARC_FRAME_LZW_FRAME_HPP

#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    body_encode_error,
    body_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct LzwFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t code_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::LzwEncodeError encode_error{
        dictionary::internal::LzwEncodeError::none};
    dictionary::internal::LzwDecodeError decode_error{
        dictionary::internal::LzwDecodeError::none};
    dictionary::internal::LzwValidationError validation_error{
        dictionary::internal::LzwValidationError::none};
    dictionary::internal::LzwFormatError format_error{
        dictionary::internal::LzwFormatError::none};
    LzwFrameCodecError error{LzwFrameCodecError::none};
};

[[nodiscard]] LzwFrameCodecResult plan_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzwFrameCodecResult encode_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzwFrameCodecResult validate_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwPhraseEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzwFrameCodecResult decode_lzw_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwPhraseEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

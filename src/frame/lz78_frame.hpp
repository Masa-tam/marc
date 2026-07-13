#ifndef MARC_FRAME_LZ78_FRAME_HPP
#define MARC_FRAME_LZ78_FRAME_HPP

#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78FrameCodecError : std::uint8_t {
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

struct Lz78FrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::Lz78EncodeError encode_error{
        dictionary::internal::Lz78EncodeError::none};
    dictionary::internal::Lz78DecodeError decode_error{
        dictionary::internal::Lz78DecodeError::none};
    dictionary::internal::Lz78ValidationError validation_error{
        dictionary::internal::Lz78ValidationError::none};
    dictionary::internal::Lz78FormatError format_error{
        dictionary::internal::Lz78FormatError::none};
    Lz78FrameCodecError error{Lz78FrameCodecError::none};
};

[[nodiscard]] Lz78FrameCodecResult plan_lz78_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] Lz78FrameCodecResult encode_lz78_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] Lz78FrameCodecResult validate_lz78_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78PhraseEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] Lz78FrameCodecResult decode_lz78_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78PhraseEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

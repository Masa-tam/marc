#ifndef MARC_FRAME_LZ77_FRAME_HPP
#define MARC_FRAME_LZ77_FRAME_HPP

#include "dictionary/lz77_decoder.hpp"
#include "dictionary/lz77_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77FrameCodecError : std::uint8_t {
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

struct Lz77FrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::Lz77EncodeError encode_error{
        dictionary::internal::Lz77EncodeError::none};
    dictionary::internal::Lz77DecodeError decode_error{
        dictionary::internal::Lz77DecodeError::none};
    dictionary::internal::Lz77ValidationError validation_error{
        dictionary::internal::Lz77ValidationError::none};
    dictionary::internal::Lz77FormatError format_error{
        dictionary::internal::Lz77FormatError::none};
    Lz77FrameCodecError error{Lz77FrameCodecError::none};
};

[[nodiscard]] Lz77FrameCodecResult plan_lz77_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] Lz77FrameCodecResult encode_lz77_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] Lz77FrameCodecResult validate_lz77_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] Lz77FrameCodecResult decode_lz77_frame(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

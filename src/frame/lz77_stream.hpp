#ifndef MARC_FRAME_LZ77_STREAM_HPP
#define MARC_FRAME_LZ77_STREAM_HPP

#include "frame/lz77_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77StreamCodecError : std::uint8_t {
    none,
    invalid_stream_header,
    invalid_parameters,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_stream,
    trailing_stream_bytes,
    frame_error,
    arithmetic_overflow,
    internal_error,
};

struct Lz77StreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::Lz77FormatError parameter_error{
        dictionary::internal::Lz77FormatError::none};
    Lz77FrameCodecError frame_error{Lz77FrameCodecError::none};
    Lz77StreamCodecError error{Lz77StreamCodecError::none};
};

[[nodiscard]] Lz77StreamCodecResult plan_lz77_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] Lz77StreamCodecResult encode_lz77_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<std::byte> output) noexcept;

[[nodiscard]] Lz77StreamCodecResult decode_lz77_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::Lz77Parameters& parameters) noexcept;

} // namespace marc::frame

#endif

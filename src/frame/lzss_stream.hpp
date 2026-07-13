#ifndef MARC_FRAME_LZSS_STREAM_HPP
#define MARC_FRAME_LZSS_STREAM_HPP

#include "frame/lzss_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssStreamCodecError : std::uint8_t {
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

struct LzssStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::LzssFormatError parameter_error{
        dictionary::internal::LzssFormatError::none};
    LzssFrameCodecError frame_error{LzssFrameCodecError::none};
    LzssStreamCodecError error{LzssStreamCodecError::none};
};

[[nodiscard]] LzssStreamCodecResult plan_lzss_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] LzssStreamCodecResult encode_lzss_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzssStreamCodecResult decode_lzss_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::LzssParameters& parameters) noexcept;

} // namespace marc::frame

#endif

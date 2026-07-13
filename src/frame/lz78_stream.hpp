#ifndef MARC_FRAME_LZ78_STREAM_HPP
#define MARC_FRAME_LZ78_STREAM_HPP

#include "frame/lz78_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t lz78_stream_prefix_size =
    stream_header_size + dictionary::internal::lz78_parameter_size;

enum class Lz78StreamCodecError : std::uint8_t {
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

struct Lz78StreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::Lz78FormatError parameter_error{
        dictionary::internal::Lz78FormatError::none};
    Lz78FrameCodecError frame_error{Lz78FrameCodecError::none};
    Lz78StreamCodecError error{Lz78StreamCodecError::none};
};

[[nodiscard]] Lz78StreamCodecResult plan_lz78_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] Lz78StreamCodecResult encode_lz78_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz78Parameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] Lz78StreamCodecResult decode_lz78_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<dictionary::internal::Lz78PhraseEntry> dictionary_workspace,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::Lz78Parameters& parameters) noexcept;

} // namespace marc::frame

#endif

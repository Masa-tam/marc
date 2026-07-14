#ifndef MARC_FRAME_LZW_STREAM_HPP
#define MARC_FRAME_LZW_STREAM_HPP

#include "frame/lzw_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t lzw_stream_prefix_size =
    stream_header_size + dictionary::internal::lzw_parameter_size;

enum class LzwStreamCodecError : std::uint8_t {
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

struct LzwStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::LzwFormatError parameter_error{
        dictionary::internal::LzwFormatError::none};
    LzwFrameCodecError frame_error{LzwFrameCodecError::none};
    LzwStreamCodecError error{LzwStreamCodecError::none};
};

[[nodiscard]] LzwStreamCodecResult plan_lzw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzwStreamCodecResult encode_lzw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzwParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzwStreamCodecResult decode_lzw_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzwPhraseEntry> dictionary_workspace,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::LzwParameters& parameters) noexcept;

} // namespace marc::frame

#endif

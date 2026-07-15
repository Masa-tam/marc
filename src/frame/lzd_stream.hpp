#ifndef MARC_FRAME_LZD_STREAM_HPP
#define MARC_FRAME_LZD_STREAM_HPP

#include "frame/lzd_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t lzd_stream_prefix_size =
    stream_header_size + dictionary::internal::lzd_parameter_size;

enum class LzdStreamCodecError : std::uint8_t {
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

struct LzdStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::LzdFormatError parameter_error{
        dictionary::internal::LzdFormatError::none};
    LzdFrameCodecError frame_error{LzdFrameCodecError::none};
    LzdStreamCodecError error{LzdStreamCodecError::none};
};

[[nodiscard]] LzdStreamCodecResult plan_lzd_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::LzdEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzdStreamCodecResult encode_lzd_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::LzdEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzdStreamCodecResult decode_lzd_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::LzdParameters& parameters) noexcept;

} // namespace marc::frame

#endif

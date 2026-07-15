#ifndef MARC_FRAME_LZMW_STREAM_HPP
#define MARC_FRAME_LZMW_STREAM_HPP

#include "frame/lzmw_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t lzmw_stream_prefix_size =
    stream_header_size + dictionary::internal::lzmw_parameter_size;

enum class LzmwStreamCodecError : std::uint8_t {
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

struct LzmwStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::LzmwFormatError parameter_error{
        dictionary::internal::LzmwFormatError::none};
    LzmwFrameCodecError frame_error{LzmwFrameCodecError::none};
    LzmwStreamCodecError error{LzmwStreamCodecError::none};
};

[[nodiscard]] LzmwStreamCodecResult plan_lzmw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry>
        dictionary_workspace) noexcept;

[[nodiscard]] LzmwStreamCodecResult encode_lzmw_stream(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzmwStreamCodecResult decode_lzmw_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output, StreamHeader& stream,
    dictionary::internal::LzmwParameters& parameters) noexcept;

} // namespace marc::frame

#endif

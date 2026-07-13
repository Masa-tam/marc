#ifndef MARC_FRAME_LZSS_FRAME_HPP
#define MARC_FRAME_LZSS_FRAME_HPP

#include "dictionary/lzss_decoder.hpp"
#include "dictionary/lzss_encoder.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssFrameCodecError : std::uint8_t {
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

struct LzssFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    FrameHeaderError header_error{FrameHeaderError::none};
    dictionary::internal::LzssEncodeError encode_error{
        dictionary::internal::LzssEncodeError::none};
    dictionary::internal::LzssDecodeError decode_error{
        dictionary::internal::LzssDecodeError::none};
    dictionary::internal::LzssValidationError validation_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError format_error{
        dictionary::internal::LzssFormatError::none};
    LzssFrameCodecError error{LzssFrameCodecError::none};
};

[[nodiscard]] LzssFrameCodecResult plan_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] LzssFrameCodecResult encode_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] LzssFrameCodecResult validate_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] LzssFrameCodecResult decode_lzss_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

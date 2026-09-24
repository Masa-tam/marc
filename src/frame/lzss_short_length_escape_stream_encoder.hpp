#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_STREAM_ENCODER_HPP

#include "frame/lzss_short_length_escape_frame_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

struct LzssShortLengthEscapeFrameTokens {
    std::span<const dictionary::internal::LzssTypedToken> tokens{};
};

enum class LzssShortLengthEscapeStreamEncodeError : std::uint8_t {
    none,
    invalid_stream,
    frame_count_mismatch,
    frame_error,
    output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    internal_error,
};

struct LzssShortLengthEscapeStreamEncodeResult {
    std::size_t serialized_size{};
    std::uint64_t frame_count{};
    std::size_t frame_index{};
    LzssShortMatchPreflightError stream_error{
        LzssShortMatchPreflightError::none};
    LzssShortMatchFrameEncodeResult frame{};
    LzssShortLengthEscapeStreamEncodeError error{
        LzssShortLengthEscapeStreamEncodeError::none};
};

// Private one-shot writer from complete, caller-owned typed-token frames.
// Views, tokens and operation storage must be stable and disjoint from output.
// No published selector calls this entry point.
[[nodiscard]] LzssShortLengthEscapeStreamEncodeResult
plan_lzss_short_length_escape_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const LzssShortLengthEscapeFrameTokens> frames,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortLengthEscapeStreamEncodeResult
encode_lzss_short_length_escape_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const LzssShortLengthEscapeFrameTokens> frames,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif

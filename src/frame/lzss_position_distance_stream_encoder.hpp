#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_STREAM_ENCODER_HPP

#include "frame/lzss_position_distance_frame_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <array>

namespace marc::frame::internal {

// Private shared header writer; validates before publishing and preserves
// bytes on failure. No public format admission is implied.
[[nodiscard]] bool serialize_lzss_position_distance_stream_header(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::array<std::byte, typed_context_stream_header_size>& bytes) noexcept;

struct LzssPositionDistanceFrameTokens {
    std::span<const dictionary::internal::LzssTypedToken> tokens{};
};

enum class LzssPositionDistanceStreamEncodeError : std::uint8_t {
    none,
    invalid_stream,
    frame_count_mismatch,
    frame_error,
    output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    internal_error,
};

struct LzssPositionDistanceStreamEncodeResult {
    std::size_t serialized_size{};
    std::uint64_t frame_count{};
    std::size_t frame_index{};
    LzssShortMatchPreflightError stream_error{
        LzssShortMatchPreflightError::none};
    LzssShortMatchFrameEncodeResult frame{};
    LzssPositionDistanceStreamEncodeError error{
        LzssPositionDistanceStreamEncodeError::none};
};

// Private one-shot writer from complete, caller-owned typed-token frames.
// Views, tokens and operation storage must be stable and disjoint from output.
// All frame plans are validated before output; unexpected write failures may
// leave partial frame bytes, but never publish the stream header. Only a success
// result makes output consumable. No published selector calls this entry point.
[[nodiscard]] LzssPositionDistanceStreamEncodeResult
plan_lzss_position_distance_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const LzssPositionDistanceFrameTokens> frames,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssPositionDistanceStreamEncodeResult
encode_lzss_position_distance_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const LzssPositionDistanceFrameTokens> frames,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif


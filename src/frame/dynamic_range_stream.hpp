#ifndef MARC_FRAME_DYNAMIC_RANGE_STREAM_HPP
#define MARC_FRAME_DYNAMIC_RANGE_STREAM_HPP

#include "core/limits.hpp"
#include "frame/dynamic_range_frame.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class DynamicRangeStreamCodecError : std::uint8_t {
    none,
    invalid_stream_header,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_stream,
    trailing_stream_bytes,
    frame_error,
    arithmetic_overflow,
    internal_error,
};

struct DynamicRangeStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    DynamicRangeFrameCodecError frame_error{
        DynamicRangeFrameCodecError::none};
    DynamicRangeStreamCodecError error{
        DynamicRangeStreamCodecError::none};
};

[[nodiscard]] DynamicRangeStreamCodecResult plan_dynamic_range_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] DynamicRangeStreamCodecResult encode_dynamic_range_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] DynamicRangeStreamCodecResult decode_dynamic_range_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> output, StreamHeader& stream) noexcept;

} // namespace marc::frame

#endif

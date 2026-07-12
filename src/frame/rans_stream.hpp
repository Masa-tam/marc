#ifndef MARC_FRAME_RANS_STREAM_HPP
#define MARC_FRAME_RANS_STREAM_HPP

#include "core/limits.hpp"
#include "frame/rans_frame.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class RansStreamCodecError : std::uint8_t {
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

struct RansStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    RansFrameCodecError frame_error{
        RansFrameCodecError::none};
    RansStreamCodecError error{
        RansStreamCodecError::none};
};

[[nodiscard]] RansStreamCodecResult plan_rans_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] RansStreamCodecResult encode_rans_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] RansStreamCodecResult decode_rans_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::span<entropy::internal::RansBlockView> views,
    StreamHeader& stream) noexcept;

} // namespace marc::frame

#endif

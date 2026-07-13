#ifndef MARC_FRAME_TANS_STREAM_HPP
#define MARC_FRAME_TANS_STREAM_HPP

#include "core/limits.hpp"
#include "frame/tans_frame.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class TansStreamCodecError : std::uint8_t {
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

struct TansStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    TansFrameCodecError frame_error{
        TansFrameCodecError::none};
    TansStreamCodecError error{
        TansStreamCodecError::none};
};

[[nodiscard]] TansStreamCodecResult plan_tans_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] TansStreamCodecResult encode_tans_stream(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] TansStreamCodecResult decode_tans_stream(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> output,
    std::span<entropy::internal::TansBlockView> views,
    StreamHeader& stream) noexcept;

} // namespace marc::frame

#endif

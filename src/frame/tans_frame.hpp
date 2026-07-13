#ifndef MARC_FRAME_TANS_FRAME_HPP
#define MARC_FRAME_TANS_FRAME_HPP

#include "core/limits.hpp"
#include "entropy/tans_controller.hpp"
#include "entropy/tans_decoder.hpp"
#include "entropy/tans_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class TansFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    views_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    controller_error,
    body_encode_error,
    body_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct TansFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::TansControllerError controller_error{
        entropy::internal::TansControllerError::none};
    entropy::internal::TansEncodeError encode_error{
        entropy::internal::TansEncodeError::none};
    entropy::internal::TansDecodeError decode_error{
        entropy::internal::TansDecodeError::none};
    TansFrameCodecError error{TansFrameCodecError::none};
};

[[nodiscard]] TansFrameCodecResult plan_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] TansFrameCodecResult encode_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] TansFrameCodecResult validate_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::TansBlockView> views) noexcept;

[[nodiscard]] TansFrameCodecResult decode_tans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output,
    std::span<entropy::internal::TansBlockView> views) noexcept;

} // namespace marc::frame

#endif

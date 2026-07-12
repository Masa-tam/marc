#ifndef MARC_FRAME_RANS_FRAME_HPP
#define MARC_FRAME_RANS_FRAME_HPP

#include "core/limits.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "entropy/rans_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class RansFrameCodecError : std::uint8_t {
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

struct RansFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansEncodeError encode_error{
        entropy::internal::RansEncodeError::none};
    entropy::internal::RansDecodeError decode_error{
        entropy::internal::RansDecodeError::none};
    RansFrameCodecError error{RansFrameCodecError::none};
};

[[nodiscard]] RansFrameCodecResult plan_rans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] RansFrameCodecResult encode_rans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] RansFrameCodecResult validate_rans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views) noexcept;

[[nodiscard]] RansFrameCodecResult decode_rans_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output,
    std::span<entropy::internal::RansBlockView> views) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_DYNAMIC_RANGE_FRAME_HPP
#define MARC_FRAME_DYNAMIC_RANGE_FRAME_HPP

#include "core/limits.hpp"
#include "entropy/dynamic_range_decoder.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class DynamicRangeFrameCodecError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    descriptor_error,
    body_encode_error,
    body_decode_error,
    arithmetic_overflow,
    internal_error,
};

struct DynamicRangeFrameCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::DynamicRangeFormatError descriptor_error{
        entropy::internal::DynamicRangeFormatError::none};
    entropy::internal::DynamicRangeEncodeError encode_error{
        entropy::internal::DynamicRangeEncodeError::none};
    entropy::internal::DynamicRangeDecodeError decode_error{
        entropy::internal::DynamicRangeDecodeError::none};
    DynamicRangeFrameCodecError error{DynamicRangeFrameCodecError::none};
};

[[nodiscard]] DynamicRangeFrameCodecResult plan_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] DynamicRangeFrameCodecResult encode_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] DynamicRangeFrameCodecResult decode_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input, std::span<std::byte> output) noexcept;

[[nodiscard]] DynamicRangeFrameCodecResult validate_dynamic_range_frame(
    const StreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t expected_sequence, std::uint64_t output_already_committed,
    std::span<const std::byte> input) noexcept;

} // namespace marc::frame

#endif

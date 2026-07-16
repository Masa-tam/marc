#ifndef MARC_FRAME_CHECKSUM_RAW_STREAM_HPP
#define MARC_FRAME_CHECKSUM_RAW_STREAM_HPP

#include "frame/frame_checksum.hpp"
#include "frame/frame_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t checksum_raw_stream_prefix_size =
    stream_header_size + hash_descriptor_size;

enum class ChecksumRawStreamError : std::uint8_t {
    none,
    invalid_stream_header,
    invalid_descriptor_region,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    truncated_stream,
    trailing_stream_bytes,
    frame_header_error,
    checksum_error,
    arithmetic_overflow,
    internal_error,
};

struct ChecksumRawStreamResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    HashDescriptorRegionError descriptor_error{
        HashDescriptorRegionError::none};
    FrameHeaderError frame_header_error{FrameHeaderError::none};
    FrameChecksumError checksum_error{FrameChecksumError::none};
    ChecksumRawStreamError error{ChecksumRawStreamError::none};
};

[[nodiscard]] ChecksumRawStreamResult plan_checksum_raw_stream_v1_1(
    const StreamHeader& stream,
    std::span<const HashDescriptor> descriptors,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input) noexcept;

[[nodiscard]] ChecksumRawStreamResult encode_checksum_raw_stream_v1_1(
    const StreamHeader& stream,
    std::span<const HashDescriptor> descriptors,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input,
    std::span<std::byte> output) noexcept;

[[nodiscard]] ChecksumRawStreamResult decode_checksum_raw_stream_v1_1(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<std::byte> output,
    StreamHeader& stream,
    HashDescriptor& descriptor) noexcept;

} // namespace marc::frame

#endif

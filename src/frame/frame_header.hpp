#ifndef MARC_FRAME_FRAME_HEADER_HPP
#define MARC_FRAME_FRAME_HEADER_HPP

#include "core/limits.hpp"
#include "frame/hash_descriptor.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t frame_header_size = 56;

struct FrameHeader {
    std::uint16_t flags{};
    std::uint64_t sequence{};
    std::uint32_t uncompressed_size{};
    std::uint32_t dictionary_serialized_size{};
    std::uint32_t compressed_payload_size{};
    std::uint32_t entropy_block_count{};
    std::uint32_t block_descriptors_size{};
    std::uint32_t checksum_trailer_size{};
};

struct FrameValidationContext {
    const StreamHeader& stream;
    const core::DecoderLimits& limits;
    std::uint64_t expected_sequence{};
    std::uint64_t output_already_committed{};
    std::span<const HashDescriptor> hash_descriptors{};
};

enum class FrameHeaderError : std::uint8_t {
    none,
    invalid_magic,
    invalid_header_size,
    unknown_flags,
    unexpected_sequence,
    unexpected_frame_size,
    contradictory_sizes,
    unsupported_feature,
    limit_exceeded,
    arithmetic_overflow,
    nonzero_reserved,
    unsupported_stream_version,
    invalid_checksum_profile,
};

[[nodiscard]] FrameHeaderError validate_frame_header(
    const FrameHeader& header,
    const FrameValidationContext& context) noexcept;

[[nodiscard]] FrameHeaderError parse_frame_header(
    std::span<const std::byte, frame_header_size> input,
    const FrameValidationContext& context,
    FrameHeader& header) noexcept;

[[nodiscard]] FrameHeaderError serialize_frame_header(
    const FrameHeader& header,
    const FrameValidationContext& context,
    std::span<std::byte, frame_header_size> output) noexcept;

[[nodiscard]] FrameHeaderError validate_frame_header_v1_1(
    const FrameHeader& header,
    const FrameValidationContext& context) noexcept;

[[nodiscard]] FrameHeaderError parse_frame_header_v1_1(
    std::span<const std::byte, frame_header_size> input,
    const FrameValidationContext& context,
    FrameHeader& header) noexcept;

[[nodiscard]] FrameHeaderError serialize_frame_header_v1_1(
    const FrameHeader& header,
    const FrameValidationContext& context,
    std::span<std::byte, frame_header_size> output) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_FRAME_CHECKSUM_HPP
#define MARC_FRAME_FRAME_CHECKSUM_HPP

#include "frame/hash_descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t frame_checksum_trailer_size = 4;

enum class FrameChecksumError : std::uint8_t {
    none,
    invalid_descriptor_set,
    invalid_trailer_size,
    hash_failure,
    checksum_mismatch,
};

[[nodiscard]] FrameChecksumError validate_frame_checksum_profile_v1_1(
    std::span<const HashDescriptor> descriptors,
    std::uint32_t declared_trailer_size) noexcept;

[[nodiscard]] FrameChecksumError generate_frame_checksum_v1_1(
    std::span<const std::byte> uncompressed_bytes,
    std::span<const HashDescriptor> descriptors,
    std::uint32_t declared_trailer_size,
    std::span<std::byte> trailer_output) noexcept;

[[nodiscard]] FrameChecksumError verify_frame_checksum_v1_1(
    std::span<const std::byte> uncompressed_bytes,
    std::span<const HashDescriptor> descriptors,
    std::uint32_t declared_trailer_size,
    std::span<const std::byte> trailer) noexcept;

} // namespace marc::frame

#endif

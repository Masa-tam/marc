#include "frame/frame_checksum.hpp"

#include "core/crc32c.hpp"

#include <algorithm>
#include <array>

namespace marc::frame {

FrameChecksumError validate_frame_checksum_profile_v1_1(
    const std::span<const HashDescriptor> descriptors,
    const std::uint32_t declared_trailer_size) noexcept {
    if (descriptors.size() != 1) {
        return FrameChecksumError::invalid_descriptor_set;
    }
    const auto& descriptor = descriptors.front();
    if (validate_hash_descriptor(descriptor) != HashDescriptorError::none
        || descriptor.algorithm_id != core::crc32c_algorithm_id
        || descriptor.target != HashTarget::uncompressed_bytes
        || descriptor.scope != HashScope::per_frame) {
        return FrameChecksumError::invalid_descriptor_set;
    }
    if (declared_trailer_size != frame_checksum_trailer_size) {
        return FrameChecksumError::invalid_trailer_size;
    }
    return FrameChecksumError::none;
}

FrameChecksumError generate_frame_checksum_v1_1(
    const std::span<const std::byte> uncompressed_bytes,
    const std::span<const HashDescriptor> descriptors,
    const std::uint32_t declared_trailer_size,
    const std::span<std::byte> trailer_output) noexcept {
    const auto validation = validate_frame_checksum_profile_v1_1(
        descriptors, declared_trailer_size);
    if (validation != FrameChecksumError::none) {
        return validation;
    }
    if (trailer_output.size() != frame_checksum_trailer_size) {
        return FrameChecksumError::invalid_trailer_size;
    }

    core::Crc32c checksum;
    if (!checksum.update(uncompressed_bytes)
        || !checksum.finalize(trailer_output)) {
        return FrameChecksumError::hash_failure;
    }
    return FrameChecksumError::none;
}

FrameChecksumError verify_frame_checksum_v1_1(
    const std::span<const std::byte> uncompressed_bytes,
    const std::span<const HashDescriptor> descriptors,
    const std::uint32_t declared_trailer_size,
    const std::span<const std::byte> trailer) noexcept {
    const auto validation = validate_frame_checksum_profile_v1_1(
        descriptors, declared_trailer_size);
    if (validation != FrameChecksumError::none) {
        return validation;
    }
    if (trailer.size() != frame_checksum_trailer_size) {
        return FrameChecksumError::invalid_trailer_size;
    }

    std::array<std::byte, frame_checksum_trailer_size> expected{};
    core::Crc32c checksum;
    if (!checksum.update(uncompressed_bytes)
        || !checksum.finalize(expected)) {
        return FrameChecksumError::hash_failure;
    }
    return std::ranges::equal(expected, trailer)
        ? FrameChecksumError::none
        : FrameChecksumError::checksum_mismatch;
}

} // namespace marc::frame

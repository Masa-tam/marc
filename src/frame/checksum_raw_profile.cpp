#include "frame/checksum_raw_profile.hpp"

#include "core/checked_math.hpp"
#include "core/crc32c.hpp"
#include "frame/frame_checksum.hpp"
#include "frame/frame_header.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace marc::frame {
namespace {

constexpr HashDescriptor canonical_descriptor{
    core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 4, 0};

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool add_frame_overhead(const std::uint64_t payload,
                                      std::uint64_t& total) noexcept {
    return core::checked_add(
               static_cast<std::uint64_t>(frame_header_size), payload, total)
        && core::checked_add(
               total, static_cast<std::uint64_t>(frame_checksum_trailer_size),
               total);
}

} // namespace

ChecksumRawProfileError make_checksum_raw_profile_v1_1(
    const ChecksumRawProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    HashDescriptor& descriptor,
    ChecksumRawWorkspaceRequirements& workspace) noexcept {
    stream = {};
    descriptor = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return ChecksumRawProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || hash_descriptor_size > limits.max_internal_buffered_bytes) {
        return ChecksumRawProfileError::limit_exceeded;
    }

    stream.minor_version = hash_format_minor_version;
    stream.frame_size = config.frame_size;
    stream.hash_descriptors_size = hash_descriptor_size;
    stream.original_size = config.original_size;
    const std::array descriptors{canonical_descriptor};
    if (validate_stream_header_v1_1(stream, limits)
            != StreamHeaderError::none
        || validate_hash_descriptor_region(descriptors)
               != HashDescriptorRegionError::none
        || validate_frame_checksum_profile_v1_1(
               descriptors, frame_checksum_trailer_size)
               != FrameChecksumError::none) {
        stream = {};
        return ChecksumRawProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame != 0) {
        std::uint64_t serialized_bytes{};
        if (!add_frame_overhead(largest_frame, serialized_bytes)) {
            stream = {};
            return ChecksumRawProfileError::arithmetic_overflow;
        }
        if (largest_frame > limits.max_compressed_payload_size
            || largest_frame > limits.max_dictionary_serialized_size
            || serialized_bytes > limits.max_internal_buffered_bytes) {
            stream = {};
            return ChecksumRawProfileError::limit_exceeded;
        }
        if (!to_size(serialized_bytes, workspace.serialized_frame_bytes)) {
            stream = {};
            workspace = {};
            return ChecksumRawProfileError::arithmetic_overflow;
        }
    }
    descriptor = canonical_descriptor;
    return ChecksumRawProfileError::none;
}

ChecksumRawProfileError calculate_checksum_raw_decoder_workspace_v1_1(
    const core::DecoderLimits& limits,
    ChecksumRawWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return ChecksumRawProfileError::invalid_configuration;
    }
    const auto maximum_payload = std::min(
        {limits.max_frame_size, limits.max_compressed_payload_size,
         limits.max_dictionary_serialized_size,
         static_cast<std::uint64_t>(
             std::numeric_limits<std::uint32_t>::max())});
    std::uint64_t maximum_serialized{};
    if (!add_frame_overhead(maximum_payload, maximum_serialized)) {
        return ChecksumRawProfileError::arithmetic_overflow;
    }
    maximum_serialized = std::min(
        maximum_serialized, limits.max_internal_buffered_bytes);
    if (!to_size(maximum_serialized, workspace.serialized_frame_bytes)) {
        workspace = {};
        return ChecksumRawProfileError::arithmetic_overflow;
    }
    return ChecksumRawProfileError::none;
}

core::ErrorCode checksum_raw_profile_error_code(
    const ChecksumRawProfileError error) noexcept {
    switch (error) {
    case ChecksumRawProfileError::none: return core::ErrorCode::none;
    case ChecksumRawProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case ChecksumRawProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case ChecksumRawProfileError::limit_exceeded:
    case ChecksumRawProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame

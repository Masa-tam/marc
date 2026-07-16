#ifndef MARC_FRAME_CHECKSUM_RAW_PROFILE_HPP
#define MARC_FRAME_CHECKSUM_RAW_PROFILE_HPP

#include "core/status.hpp"
#include "frame/hash_descriptor.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct ChecksumRawProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
};

struct ChecksumRawWorkspaceRequirements {
    std::size_t serialized_frame_bytes{};
};

enum class ChecksumRawProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] ChecksumRawProfileError make_checksum_raw_profile_v1_1(
    const ChecksumRawProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    HashDescriptor& descriptor,
    ChecksumRawWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] ChecksumRawProfileError
calculate_checksum_raw_decoder_workspace_v1_1(
    const core::DecoderLimits& limits,
    ChecksumRawWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode checksum_raw_profile_error_code(
    ChecksumRawProfileError error) noexcept;

} // namespace marc::frame

#endif

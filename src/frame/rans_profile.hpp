#ifndef MARC_FRAME_RANS_PROFILE_HPP
#define MARC_FRAME_RANS_PROFILE_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct RansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t block_size{UINT32_C(1) << 16};
};

struct RansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct RansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class RansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] RansProfileError make_rans_profile(
    const RansProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    RansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] RansProfileError calculate_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    RansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode rans_profile_error_code(
    RansProfileError error) noexcept;

} // namespace marc::frame

#endif

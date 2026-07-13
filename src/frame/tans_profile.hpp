#ifndef MARC_FRAME_TANS_PROFILE_HPP
#define MARC_FRAME_TANS_PROFILE_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct TansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t block_size{UINT32_C(1) << 16};
};

struct TansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct TansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class TansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] TansProfileError make_tans_profile(
    const TansProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    TansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] TansProfileError calculate_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    TansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode tans_profile_error_code(
    TansProfileError error) noexcept;

} // namespace marc::frame

#endif

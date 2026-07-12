#ifndef MARC_FRAME_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_DYNAMIC_RANGE_PROFILE_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct DynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
};

struct DynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct DynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class DynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] DynamicRangeProfileError make_dynamic_range_profile(
    const DynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    DynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] DynamicRangeProfileError
calculate_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    DynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode dynamic_range_profile_error_code(
    DynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

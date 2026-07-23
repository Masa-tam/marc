#ifndef MARC_FRAME_LZSS_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZSS_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzss_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssDynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssDynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssDynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class LzssDynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssDynamicRangeProfileError make_lzss_dynamic_range_profile(
    const LzssDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssDynamicRangeProfileError
calculate_lzss_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_dynamic_range_profile_error_code(
    LzssDynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

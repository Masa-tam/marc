#ifndef MARC_FRAME_LZ77_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZ77_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz77_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77DynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77DynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77DynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class Lz77DynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77DynamicRangeProfileError make_lz77_dynamic_range_profile(
    const Lz77DynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77DynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77DynamicRangeProfileError
calculate_lz77_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77DynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_dynamic_range_profile_error_code(
    Lz77DynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

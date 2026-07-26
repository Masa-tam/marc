#ifndef MARC_FRAME_LZD_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZD_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzd_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzdDynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdDynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzdDynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzdDynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzdDynamicRangeWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzdDynamicRangeEncoderViews {
    std::span<dictionary::internal::LzdEncoderEntry> entries{};
};

struct LzdDynamicRangeDecoderViews {
    std::span<dictionary::internal::LzdPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzdDynamicRangeProfileError make_lzd_dynamic_range_profile(
    const LzdDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzdDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdDynamicRangeProfileError
calculate_lzd_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdDynamicRangeWorkspaceError
partition_lzd_dynamic_range_encoder_views(
    const LzdDynamicRangeEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdDynamicRangeEncoderViews& views) noexcept;

[[nodiscard]] LzdDynamicRangeWorkspaceError
partition_lzd_dynamic_range_decoder_views(
    const LzdDynamicRangeDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdDynamicRangeDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzd_dynamic_range_profile_error_code(
    LzdDynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

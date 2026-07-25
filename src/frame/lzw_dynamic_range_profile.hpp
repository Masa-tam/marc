#ifndef MARC_FRAME_LZW_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZW_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzw_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzwDynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzwParameters parameters{};
};

struct LzwDynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzwDynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzwDynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzwDynamicRangeWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzwDynamicRangeEncoderViews {
    std::span<dictionary::internal::LzwEncoderEntry> entries{};
};

struct LzwDynamicRangeDecoderViews {
    std::span<dictionary::internal::LzwPhraseEntry> phrases{};
};

[[nodiscard]] LzwDynamicRangeProfileError make_lzw_dynamic_range_profile(
    const LzwDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwDynamicRangeProfileError
calculate_lzw_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwDynamicRangeWorkspaceError
partition_lzw_dynamic_range_encoder_views(
    const LzwDynamicRangeEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwDynamicRangeEncoderViews& views) noexcept;

[[nodiscard]] LzwDynamicRangeWorkspaceError
partition_lzw_dynamic_range_decoder_views(
    const LzwDynamicRangeDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwDynamicRangeDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzw_dynamic_range_profile_error_code(
    LzwDynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

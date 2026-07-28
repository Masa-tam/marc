#ifndef MARC_FRAME_LZMW_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZMW_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzmw_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzmwDynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwDynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzmwDynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzmwDynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzmwDynamicRangeWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzmwDynamicRangeEncoderViews {
    std::span<dictionary::internal::LzmwEncoderEntry> entries{};
};

struct LzmwDynamicRangeDecoderViews {
    std::span<dictionary::internal::LzmwPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzmwDynamicRangeProfileError
make_lzmw_dynamic_range_profile(
    const LzmwDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwDynamicRangeProfileError
calculate_lzmw_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwDynamicRangeWorkspaceError
partition_lzmw_dynamic_range_encoder_views(
    const LzmwDynamicRangeEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwDynamicRangeEncoderViews& views) noexcept;

[[nodiscard]] LzmwDynamicRangeWorkspaceError
partition_lzmw_dynamic_range_decoder_views(
    const LzmwDynamicRangeDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwDynamicRangeDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzmw_dynamic_range_profile_error_code(
    LzmwDynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

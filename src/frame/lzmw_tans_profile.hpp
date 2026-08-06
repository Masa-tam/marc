#ifndef MARC_FRAME_LZMW_TANS_PROFILE_HPP
#define MARC_FRAME_LZMW_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzmw_tans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzmwTansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwTansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzmwTansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t phrase_offset{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzmwTansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzmwTansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzmwTansEncoderViews {
    std::span<dictionary::internal::LzmwEncoderEntry> entries{};
};

struct LzmwTansDecoderViews {
    std::span<entropy::internal::TansBlockView> blocks{};
    std::span<dictionary::internal::LzmwPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzmwTansProfileError make_lzmw_tans_profile(
    const LzmwTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwTansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwTansProfileError calculate_lzmw_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwTansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwTansWorkspaceError partition_lzmw_tans_encoder_views(
    const LzmwTansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwTansEncoderViews& views) noexcept;

[[nodiscard]] LzmwTansWorkspaceError partition_lzmw_tans_decoder_views(
    const LzmwTansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwTansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzmw_tans_profile_error_code(
    LzmwTansProfileError error) noexcept;

} // namespace marc::frame

#endif

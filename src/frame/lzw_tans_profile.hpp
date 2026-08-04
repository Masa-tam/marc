#ifndef MARC_FRAME_LZW_TANS_PROFILE_HPP
#define MARC_FRAME_LZW_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzw_tans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzwTansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzwParameters parameters{};
};

struct LzwTansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzwTansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t phrase_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzwTansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzwTansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzwTansEncoderViews {
    std::span<dictionary::internal::LzwEncoderEntry> entries{};
};

struct LzwTansDecoderViews {
    std::span<entropy::internal::TansBlockView> blocks{};
    std::span<dictionary::internal::LzwPhraseEntry> phrases{};
};

[[nodiscard]] LzwTansProfileError make_lzw_tans_profile(
    const LzwTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwTansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwTansProfileError calculate_lzw_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwTansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwTansWorkspaceError partition_lzw_tans_encoder_views(
    const LzwTansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwTansEncoderViews& views) noexcept;

[[nodiscard]] LzwTansWorkspaceError partition_lzw_tans_decoder_views(
    const LzwTansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwTansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzw_tans_profile_error_code(
    LzwTansProfileError error) noexcept;

} // namespace marc::frame

#endif

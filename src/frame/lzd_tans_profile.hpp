#ifndef MARC_FRAME_LZD_TANS_PROFILE_HPP
#define MARC_FRAME_LZD_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzd_tans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzdTansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdTansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzdTansDecoderWorkspaceRequirements {
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

enum class LzdTansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzdTansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzdTansEncoderViews {
    std::span<dictionary::internal::LzdEncoderEntry> entries{};
};

struct LzdTansDecoderViews {
    std::span<entropy::internal::TansBlockView> blocks{};
    std::span<dictionary::internal::LzdPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzdTansProfileError make_lzd_tans_profile(
    const LzdTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzdTansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdTansProfileError calculate_lzd_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdTansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdTansWorkspaceError partition_lzd_tans_encoder_views(
    const LzdTansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdTansEncoderViews& views) noexcept;

[[nodiscard]] LzdTansWorkspaceError partition_lzd_tans_decoder_views(
    const LzdTansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdTansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzd_tans_profile_error_code(
    LzdTansProfileError error) noexcept;

} // namespace marc::frame

#endif

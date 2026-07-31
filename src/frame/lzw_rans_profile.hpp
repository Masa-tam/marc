#ifndef MARC_FRAME_LZW_RANS_PROFILE_HPP
#define MARC_FRAME_LZW_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzw_rans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzwRansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzwParameters parameters{};
};

struct LzwRansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzwRansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t phrase_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzwRansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzwRansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzwRansEncoderViews {
    std::span<dictionary::internal::LzwEncoderEntry> entries{};
};

struct LzwRansDecoderViews {
    std::span<entropy::internal::RansBlockView> blocks{};
    std::span<dictionary::internal::LzwPhraseEntry> phrases{};
};

[[nodiscard]] LzwRansProfileError make_lzw_rans_profile(
    const LzwRansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwRansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwRansProfileError calculate_lzw_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwRansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwRansWorkspaceError partition_lzw_rans_encoder_views(
    const LzwRansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwRansEncoderViews& views) noexcept;

[[nodiscard]] LzwRansWorkspaceError partition_lzw_rans_decoder_views(
    const LzwRansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwRansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzw_rans_profile_error_code(
    LzwRansProfileError error) noexcept;

} // namespace marc::frame

#endif

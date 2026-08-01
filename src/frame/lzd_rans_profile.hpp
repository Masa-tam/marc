#ifndef MARC_FRAME_LZD_RANS_PROFILE_HPP
#define MARC_FRAME_LZD_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzd_rans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzdRansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdRansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzdRansDecoderWorkspaceRequirements {
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

enum class LzdRansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzdRansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzdRansEncoderViews {
    std::span<dictionary::internal::LzdEncoderEntry> entries{};
};

struct LzdRansDecoderViews {
    std::span<entropy::internal::RansBlockView> blocks{};
    std::span<dictionary::internal::LzdPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzdRansProfileError make_lzd_rans_profile(
    const LzdRansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzdRansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdRansProfileError calculate_lzd_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdRansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdRansWorkspaceError partition_lzd_rans_encoder_views(
    const LzdRansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdRansEncoderViews& views) noexcept;

[[nodiscard]] LzdRansWorkspaceError partition_lzd_rans_decoder_views(
    const LzdRansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdRansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzd_rans_profile_error_code(
    LzdRansProfileError error) noexcept;

} // namespace marc::frame

#endif

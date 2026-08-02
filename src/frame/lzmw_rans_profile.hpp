#ifndef MARC_FRAME_LZMW_RANS_PROFILE_HPP
#define MARC_FRAME_LZMW_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzmw_rans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzmwRansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwRansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzmwRansDecoderWorkspaceRequirements {
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

enum class LzmwRansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzmwRansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzmwRansEncoderViews {
    std::span<dictionary::internal::LzmwEncoderEntry> entries{};
};

struct LzmwRansDecoderViews {
    std::span<entropy::internal::RansBlockView> blocks{};
    std::span<dictionary::internal::LzmwPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzmwRansProfileError make_lzmw_rans_profile(
    const LzmwRansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwRansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwRansProfileError calculate_lzmw_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwRansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwRansWorkspaceError partition_lzmw_rans_encoder_views(
    const LzmwRansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwRansEncoderViews& views) noexcept;

[[nodiscard]] LzmwRansWorkspaceError partition_lzmw_rans_decoder_views(
    const LzmwRansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwRansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzmw_rans_profile_error_code(
    LzmwRansProfileError error) noexcept;

} // namespace marc::frame

#endif

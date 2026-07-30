#ifndef MARC_FRAME_LZ78_RANS_PROFILE_HPP
#define MARC_FRAME_LZ78_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz78_rans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct Lz78RansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78RansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct Lz78RansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t phrase_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class Lz78RansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class Lz78RansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct Lz78RansEncoderViews {
    std::span<dictionary::internal::Lz78EncoderEntry> entries{};
};

struct Lz78RansDecoderViews {
    std::span<entropy::internal::RansBlockView> blocks{};
    std::span<dictionary::internal::Lz78PhraseEntry> phrases{};
};

[[nodiscard]] Lz78RansProfileError make_lz78_rans_profile(
    const Lz78RansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78RansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78RansProfileError
calculate_lz78_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78RansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78RansWorkspaceError partition_lz78_rans_encoder_views(
    const Lz78RansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78RansEncoderViews& views) noexcept;

[[nodiscard]] Lz78RansWorkspaceError partition_lz78_rans_decoder_views(
    const Lz78RansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78RansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lz78_rans_profile_error_code(
    Lz78RansProfileError error) noexcept;

} // namespace marc::frame

#endif

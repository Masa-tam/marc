#ifndef MARC_FRAME_LZ78_TANS_PROFILE_HPP
#define MARC_FRAME_LZ78_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz78_tans_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct Lz78TansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78TansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct Lz78TansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t phrase_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class Lz78TansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class Lz78TansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct Lz78TansEncoderViews {
    std::span<dictionary::internal::Lz78EncoderEntry> entries{};
};

struct Lz78TansDecoderViews {
    std::span<entropy::internal::TansBlockView> blocks{};
    std::span<dictionary::internal::Lz78PhraseEntry> phrases{};
};

[[nodiscard]] Lz78TansProfileError make_lz78_tans_profile(
    const Lz78TansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78TansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78TansProfileError
calculate_lz78_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78TansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78TansWorkspaceError partition_lz78_tans_encoder_views(
    const Lz78TansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78TansEncoderViews& views) noexcept;

[[nodiscard]] Lz78TansWorkspaceError partition_lz78_tans_decoder_views(
    const Lz78TansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78TansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lz78_tans_profile_error_code(
    Lz78TansProfileError error) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZSS_CONTEXTUAL_TANS_PROFILE_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_tans_decode_tables.hpp"
#include "frame/lzss_contextual_tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

struct LzssContextualTansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters dictionary{};
};

struct LzssContextualTansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t token_count{};
    std::size_t table_count{};
    std::size_t table_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzssContextualTansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t table_count{};
    std::size_t token_count{};
    std::size_t token_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzssContextualTansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzssContextualTansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzssContextualTansEncoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<std::uint16_t> tables{};
};

struct LzssContextualTansDecoderViews {
    std::span<entropy::internal::TansDecodeEntry> tables{};
    std::span<dictionary::internal::LzssTypedToken> tokens{};
};

[[nodiscard]] LzssContextualTansProfileError
make_lzss_contextual_tans_profile(
    const LzssContextualTansProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualTansStreamHeader& stream,
    LzssContextualTansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssContextualTansProfileError
calculate_lzss_contextual_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualTansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssContextualTansWorkspaceError
partition_lzss_contextual_tans_encoder_views(
    const LzssContextualTansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssContextualTansEncoderViews& views) noexcept;

[[nodiscard]] LzssContextualTansWorkspaceError
partition_lzss_contextual_tans_decoder_views(
    const LzssContextualTansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssContextualTansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzss_contextual_tans_profile_error_code(
    LzssContextualTansProfileError error) noexcept;

} // namespace marc::frame::internal

#endif

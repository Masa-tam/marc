#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_PROFILE_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/rans_decode_table.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualRansProfileVariant : std::uint8_t {
    field_context_64k,
    field_context_1m,
    field_context_4m,
};

struct LzssContextualRansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters dictionary{};
    LzssContextualRansProfileVariant variant{
        LzssContextualRansProfileVariant::field_context_64k};
};

struct LzssContextualRansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t token_count{};
    std::size_t match_finder_offset{};
    std::size_t match_finder_bytes{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzssContextualRansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t table_count{};
    std::size_t token_count{};
    std::size_t token_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzssContextualRansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzssContextualRansWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzssContextualRansEncoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<std::byte> match_finder{};
};

struct LzssContextualRansDecoderViews {
    std::span<entropy::internal::RansDecodeEntry> tables{};
    std::span<dictionary::internal::LzssTypedToken> tokens{};
};

[[nodiscard]] LzssContextualRansProfileError
make_lzss_contextual_rans_profile(
    const LzssContextualRansProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualRansStreamHeader& stream,
    LzssContextualRansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssContextualRansProfileError
calculate_lzss_contextual_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualRansDecoderWorkspaceRequirements& workspace,
    LzssContextualRansProfileVariant variant =
        LzssContextualRansProfileVariant::field_context_64k) noexcept;

[[nodiscard]] LzssContextualRansWorkspaceError
partition_lzss_contextual_rans_encoder_views(
    const LzssContextualRansEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssContextualRansEncoderViews& views) noexcept;

[[nodiscard]] LzssContextualRansWorkspaceError
partition_lzss_contextual_rans_decoder_views(
    const LzssContextualRansDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzssContextualRansDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzss_contextual_rans_profile_error_code(
    LzssContextualRansProfileError error) noexcept;

} // namespace marc::frame::internal

#endif

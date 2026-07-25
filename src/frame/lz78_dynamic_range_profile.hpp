#ifndef MARC_FRAME_LZ78_DYNAMIC_RANGE_PROFILE_HPP
#define MARC_FRAME_LZ78_DYNAMIC_RANGE_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz78_dynamic_range_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct Lz78DynamicRangeProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78DynamicRangeEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct Lz78DynamicRangeDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class Lz78DynamicRangeProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class Lz78DynamicRangeWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct Lz78DynamicRangeEncoderViews {
    std::span<dictionary::internal::Lz78EncoderEntry> entries{};
};

struct Lz78DynamicRangeDecoderViews {
    std::span<dictionary::internal::Lz78PhraseEntry> phrases{};
};

[[nodiscard]] Lz78DynamicRangeProfileError
make_lz78_dynamic_range_profile(
    const Lz78DynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78DynamicRangeEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78DynamicRangeProfileError
calculate_lz78_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78DynamicRangeDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78DynamicRangeWorkspaceError
partition_lz78_dynamic_range_encoder_views(
    const Lz78DynamicRangeEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78DynamicRangeEncoderViews& views) noexcept;

[[nodiscard]] Lz78DynamicRangeWorkspaceError
partition_lz78_dynamic_range_decoder_views(
    const Lz78DynamicRangeDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78DynamicRangeDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lz78_dynamic_range_profile_error_code(
    Lz78DynamicRangeProfileError error) noexcept;

} // namespace marc::frame

#endif

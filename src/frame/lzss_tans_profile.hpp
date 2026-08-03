
#ifndef MARC_FRAME_LZSS_TANS_PROFILE_HPP
#define MARC_FRAME_LZSS_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzss_tans_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssTansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssTansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssTansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class LzssTansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssTansProfileError make_lzss_tans_profile(
    const LzssTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssTansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssTansProfileError
calculate_lzss_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssTansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_tans_profile_error_code(
    LzssTansProfileError error) noexcept;

} // namespace marc::frame

#endif


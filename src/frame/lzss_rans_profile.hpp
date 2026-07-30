#ifndef MARC_FRAME_LZSS_RANS_PROFILE_HPP
#define MARC_FRAME_LZSS_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzss_rans_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssRansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssRansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssRansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class LzssRansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssRansProfileError make_lzss_rans_profile(
    const LzssRansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssRansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssRansProfileError
calculate_lzss_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssRansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_rans_profile_error_code(
    LzssRansProfileError error) noexcept;

} // namespace marc::frame

#endif

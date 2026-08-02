
#ifndef MARC_FRAME_LZ77_TANS_PROFILE_HPP
#define MARC_FRAME_LZ77_TANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz77_tans_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77TansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77TansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77TansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class Lz77TansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77TansProfileError make_lz77_tans_profile(
    const Lz77TansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77TansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77TansProfileError
calculate_lz77_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77TansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_tans_profile_error_code(
    Lz77TansProfileError error) noexcept;

} // namespace marc::frame

#endif

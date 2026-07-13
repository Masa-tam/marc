#ifndef MARC_FRAME_LZSS_PROFILE_HPP
#define MARC_FRAME_LZSS_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzss_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class LzssProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssProfileError make_lzss_profile(
    const LzssProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssProfileError calculate_lzss_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_profile_error_code(
    LzssProfileError error) noexcept;

} // namespace marc::frame

#endif

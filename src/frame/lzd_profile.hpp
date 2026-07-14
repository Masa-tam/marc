#ifndef MARC_FRAME_LZD_PROFILE_HPP
#define MARC_FRAME_LZD_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzd_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzdProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_entries{};
};

struct LzdDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
};

enum class LzdProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzdProfileError make_lzd_profile(
    const LzdProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzdEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdProfileError calculate_lzd_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzd_profile_error_code(
    LzdProfileError error) noexcept;

} // namespace marc::frame

#endif

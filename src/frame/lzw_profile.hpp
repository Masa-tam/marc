#ifndef MARC_FRAME_LZW_PROFILE_HPP
#define MARC_FRAME_LZW_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzw_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzwProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::LzwParameters parameters{};
};

struct LzwEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_entries{};
};

struct LzwDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t dictionary_entries{};
};

enum class LzwProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzwProfileError make_lzw_profile(
    const LzwProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwProfileError calculate_lzw_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzw_profile_error_code(
    LzwProfileError error) noexcept;

} // namespace marc::frame

#endif

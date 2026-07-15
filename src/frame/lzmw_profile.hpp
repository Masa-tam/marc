#ifndef MARC_FRAME_LZMW_PROFILE_HPP
#define MARC_FRAME_LZMW_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzmw_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzmwProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_entries{};
};

struct LzmwDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
};

enum class LzmwProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzmwProfileError make_lzmw_profile(
    const LzmwProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwProfileError calculate_lzmw_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzmw_profile_error_code(
    LzmwProfileError error) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZ77_PROFILE_HPP
#define MARC_FRAME_LZ77_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lz77_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77ProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77EncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77DecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class Lz77ProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77ProfileError make_lz77_profile(
    const Lz77ProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77EncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77ProfileError calculate_lz77_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77DecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_profile_error_code(
    Lz77ProfileError error) noexcept;

} // namespace marc::frame

#endif

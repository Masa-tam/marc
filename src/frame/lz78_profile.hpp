#ifndef MARC_FRAME_LZ78_PROFILE_HPP
#define MARC_FRAME_LZ78_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lz78_format.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz78ProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78EncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_entries{};
};

struct Lz78DecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t dictionary_entries{};
};

enum class Lz78ProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz78ProfileError make_lz78_profile(
    const Lz78ProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78EncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78ProfileError calculate_lz78_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78DecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz78_profile_error_code(
    Lz78ProfileError error) noexcept;

} // namespace marc::frame

#endif

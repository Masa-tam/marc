#ifndef MARC_FRAME_LZ77_RANS_PROFILE_HPP
#define MARC_FRAME_LZ77_RANS_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz77_rans_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77RansProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77RansEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77RansDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class Lz77RansProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77RansProfileError make_lz77_rans_profile(
    const Lz77RansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77RansEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77RansProfileError
calculate_lz77_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77RansDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_rans_profile_error_code(
    Lz77RansProfileError error) noexcept;

} // namespace marc::frame

#endif

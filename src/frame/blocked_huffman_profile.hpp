#ifndef MARC_FRAME_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct BlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t block_size{UINT32_C(1) << 16};
};

struct EncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct DecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class ProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] ProfileError make_blocked_huffman_profile(
    const BlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    EncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] ProfileError calculate_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    DecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode profile_error_code(ProfileError error) noexcept;

} // namespace marc::frame

#endif

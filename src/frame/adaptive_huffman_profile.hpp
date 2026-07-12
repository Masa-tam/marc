#ifndef MARC_FRAME_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct AdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
};

struct AdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct AdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class AdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] AdaptiveHuffmanProfileError make_adaptive_huffman_profile(
    const AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] AdaptiveHuffmanProfileError
calculate_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode adaptive_huffman_profile_error_code(
    AdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

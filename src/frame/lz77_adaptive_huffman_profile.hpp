#ifndef MARC_FRAME_LZ77_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZ77_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz77_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77AdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77AdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77AdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class Lz77AdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77AdaptiveHuffmanProfileError
make_lz77_adaptive_huffman_profile(
    const Lz77AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77AdaptiveHuffmanProfileError
calculate_lz77_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_adaptive_huffman_profile_error_code(
    Lz77AdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

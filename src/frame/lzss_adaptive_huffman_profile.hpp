#ifndef MARC_FRAME_LZSS_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZSS_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzss_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssAdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssAdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssAdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
};

enum class LzssAdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssAdaptiveHuffmanProfileError
make_lzss_adaptive_huffman_profile(
    const LzssAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssAdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssAdaptiveHuffmanProfileError
calculate_lzss_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssAdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_adaptive_huffman_profile_error_code(
    LzssAdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

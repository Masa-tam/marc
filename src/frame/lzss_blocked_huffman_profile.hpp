#ifndef MARC_FRAME_LZSS_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZSS_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzss_blocked_huffman_stream.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct LzssBlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters parameters{};
};

struct LzssBlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct LzssBlockedHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class LzssBlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssBlockedHuffmanProfileError
make_lzss_blocked_huffman_profile(
    const LzssBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssBlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzssBlockedHuffmanProfileError
calculate_lzss_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssBlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lzss_blocked_huffman_profile_error_code(
    LzssBlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

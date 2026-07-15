#ifndef MARC_FRAME_LZ77_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZ77_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz77_blocked_huffman_stream.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame {

struct Lz77BlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz77Parameters parameters{};
};

struct Lz77BlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
};

struct Lz77BlockedHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
};

enum class Lz77BlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] Lz77BlockedHuffmanProfileError
make_lz77_blocked_huffman_profile(
    const Lz77BlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77BlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz77BlockedHuffmanProfileError
calculate_lz77_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77BlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] core::ErrorCode lz77_blocked_huffman_profile_error_code(
    Lz77BlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

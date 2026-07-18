#ifndef MARC_FRAME_LZD_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZD_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzd_blocked_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzdBlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdBlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzdBlockedHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t phrase_offset{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzdBlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzdBlockedHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzdBlockedHuffmanEncoderViews {
    std::span<dictionary::internal::LzdEncoderEntry> entries{};
};

struct LzdBlockedHuffmanDecoderViews {
    std::span<entropy::internal::BlockedHuffmanBlockView> blocks{};
    std::span<dictionary::internal::LzdPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzdBlockedHuffmanProfileError
make_lzd_blocked_huffman_profile(
    const LzdBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    LzdBlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdBlockedHuffmanProfileError
calculate_lzd_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdBlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdBlockedHuffmanWorkspaceError
partition_lzd_blocked_huffman_encoder_views(
    const LzdBlockedHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdBlockedHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzdBlockedHuffmanWorkspaceError
partition_lzd_blocked_huffman_decoder_views(
    const LzdBlockedHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdBlockedHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzd_blocked_huffman_profile_error_code(
    LzdBlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

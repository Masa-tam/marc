#ifndef MARC_FRAME_LZW_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZW_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzw_blocked_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzwBlockedHuffmanProfileConfig {
  std::uint64_t original_size{};
  std::uint32_t frame_size{UINT32_C(1) << 20};
  std::uint32_t entropy_block_size{UINT32_C(1) << 16};
  dictionary::internal::LzwParameters parameters{};
};

struct LzwBlockedHuffmanEncoderWorkspaceRequirements {
  std::size_t frame_input_bytes{};
  std::size_t dictionary_staging_bytes{};
  std::size_t frame_encoded_bytes{};
  std::size_t encoder_entry_count{};
  std::size_t views_bytes{};
  std::size_t views_alignment{1};
};

struct LzwBlockedHuffmanDecoderWorkspaceRequirements {
  std::size_t frame_encoded_bytes{};
  std::size_t dictionary_staging_bytes{};
  std::size_t frame_decoded_bytes{};
  std::size_t block_view_count{};
  std::size_t phrase_entry_count{};
  std::size_t phrase_offset{};
  std::size_t views_bytes{};
  std::size_t views_alignment{1};
};

enum class LzwBlockedHuffmanProfileError : std::uint8_t {
  none,
  invalid_configuration,
  unsupported,
  limit_exceeded,
  arithmetic_overflow,
};

enum class LzwBlockedHuffmanWorkspaceError : std::uint8_t {
  none,
  invalid_requirements,
  too_small,
  misaligned,
  arithmetic_overflow,
};

struct LzwBlockedHuffmanEncoderViews {
  std::span<dictionary::internal::LzwEncoderEntry> entries{};
};

struct LzwBlockedHuffmanDecoderViews {
  std::span<entropy::internal::BlockedHuffmanBlockView> blocks{};
  std::span<dictionary::internal::LzwPhraseEntry> phrases{};
};

[[nodiscard]] LzwBlockedHuffmanProfileError make_lzw_blocked_huffman_profile(
    const LzwBlockedHuffmanProfileConfig &config,
    const core::DecoderLimits &limits, StreamHeader &stream,
    LzwBlockedHuffmanEncoderWorkspaceRequirements &workspace) noexcept;

[[nodiscard]] LzwBlockedHuffmanProfileError
calculate_lzw_blocked_huffman_decoder_workspace(
    const core::DecoderLimits &limits,
    LzwBlockedHuffmanDecoderWorkspaceRequirements &workspace) noexcept;

[[nodiscard]] LzwBlockedHuffmanWorkspaceError
partition_lzw_blocked_huffman_encoder_views(
    const LzwBlockedHuffmanEncoderWorkspaceRequirements &requirements,
    std::span<std::byte> storage,
    LzwBlockedHuffmanEncoderViews &views) noexcept;

[[nodiscard]] LzwBlockedHuffmanWorkspaceError
partition_lzw_blocked_huffman_decoder_views(
    const LzwBlockedHuffmanDecoderWorkspaceRequirements &requirements,
    std::span<std::byte> storage,
    LzwBlockedHuffmanDecoderViews &views) noexcept;

[[nodiscard]] core::ErrorCode lzw_blocked_huffman_profile_error_code(
    LzwBlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

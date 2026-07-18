#ifndef MARC_FRAME_LZMW_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZMW_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzmw_blocked_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzmwBlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwBlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzmwBlockedHuffmanDecoderWorkspaceRequirements {
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

enum class LzmwBlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzmwBlockedHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzmwBlockedHuffmanEncoderViews {
    std::span<dictionary::internal::LzmwEncoderEntry> entries{};
};

struct LzmwBlockedHuffmanDecoderViews {
    std::span<entropy::internal::BlockedHuffmanBlockView> blocks{};
    std::span<dictionary::internal::LzmwPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzmwBlockedHuffmanProfileError
make_lzmw_blocked_huffman_profile(
    const LzmwBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    LzmwBlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwBlockedHuffmanProfileError
calculate_lzmw_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwBlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwBlockedHuffmanWorkspaceError
partition_lzmw_blocked_huffman_encoder_views(
    const LzmwBlockedHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwBlockedHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzmwBlockedHuffmanWorkspaceError
partition_lzmw_blocked_huffman_decoder_views(
    const LzmwBlockedHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwBlockedHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzmw_blocked_huffman_profile_error_code(
    LzmwBlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

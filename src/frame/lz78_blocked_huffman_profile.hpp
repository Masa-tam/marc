#ifndef MARC_FRAME_LZ78_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZ78_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz78_blocked_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct Lz78BlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 20};
    std::uint32_t entropy_block_size{UINT32_C(1) << 16};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78BlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct Lz78BlockedHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t block_view_count{};
    std::size_t phrase_entry_count{};
    std::size_t phrase_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class Lz78BlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class Lz78BlockedHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct Lz78BlockedHuffmanEncoderViews {
    std::span<dictionary::internal::Lz78EncoderEntry> entries{};
};

struct Lz78BlockedHuffmanDecoderViews {
    std::span<entropy::internal::BlockedHuffmanBlockView> blocks{};
    std::span<dictionary::internal::Lz78PhraseEntry> phrases{};
};

[[nodiscard]] Lz78BlockedHuffmanProfileError
make_lz78_blocked_huffman_profile(
    const Lz78BlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    Lz78BlockedHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78BlockedHuffmanProfileError
calculate_lz78_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78BlockedHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78BlockedHuffmanWorkspaceError
partition_lz78_blocked_huffman_encoder_views(
    const Lz78BlockedHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78BlockedHuffmanEncoderViews& views) noexcept;

[[nodiscard]] Lz78BlockedHuffmanWorkspaceError
partition_lz78_blocked_huffman_decoder_views(
    const Lz78BlockedHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78BlockedHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lz78_blocked_huffman_profile_error_code(
    Lz78BlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

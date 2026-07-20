#ifndef MARC_FRAME_LZW_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZW_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzw_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzwAdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzwParameters parameters{};
};

struct LzwAdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzwAdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzwAdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzwAdaptiveHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzwAdaptiveHuffmanEncoderViews {
    std::span<dictionary::internal::LzwEncoderEntry> entries{};
};

struct LzwAdaptiveHuffmanDecoderViews {
    std::span<dictionary::internal::LzwPhraseEntry> phrases{};
};

[[nodiscard]] LzwAdaptiveHuffmanProfileError
make_lzw_adaptive_huffman_profile(
    const LzwAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwAdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwAdaptiveHuffmanProfileError
calculate_lzw_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwAdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzwAdaptiveHuffmanWorkspaceError
partition_lzw_adaptive_huffman_encoder_views(
    const LzwAdaptiveHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwAdaptiveHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzwAdaptiveHuffmanWorkspaceError
partition_lzw_adaptive_huffman_decoder_views(
    const LzwAdaptiveHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzwAdaptiveHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzw_adaptive_huffman_profile_error_code(
    LzwAdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

#ifndef MARC_FRAME_LZD_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZD_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzd_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzdAdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzdParameters parameters{};
};

struct LzdAdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzdAdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzdAdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzdAdaptiveHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzdAdaptiveHuffmanEncoderViews {
    std::span<dictionary::internal::LzdEncoderEntry> entries{};
};

struct LzdAdaptiveHuffmanDecoderViews {
    std::span<dictionary::internal::LzdPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzdAdaptiveHuffmanProfileError
make_lzd_adaptive_huffman_profile(
    const LzdAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzdAdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdAdaptiveHuffmanProfileError
calculate_lzd_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzdAdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzdAdaptiveHuffmanWorkspaceError
partition_lzd_adaptive_huffman_encoder_views(
    const LzdAdaptiveHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdAdaptiveHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzdAdaptiveHuffmanWorkspaceError
partition_lzd_adaptive_huffman_decoder_views(
    const LzdAdaptiveHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzdAdaptiveHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzd_adaptive_huffman_profile_error_code(
    LzdAdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

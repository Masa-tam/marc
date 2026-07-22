#ifndef MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lzmw_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct LzmwAdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzmwParameters parameters{};
};

struct LzmwAdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzmwAdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t expansion_entry_count{};
    std::size_t expansion_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzmwAdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzmwAdaptiveHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzmwAdaptiveHuffmanEncoderViews {
    std::span<dictionary::internal::LzmwEncoderEntry> entries{};
};

struct LzmwAdaptiveHuffmanDecoderViews {
    std::span<dictionary::internal::LzmwPhraseEntry> phrases{};
    std::span<std::uint32_t> expansion{};
};

[[nodiscard]] LzmwAdaptiveHuffmanProfileError
make_lzmw_adaptive_huffman_profile(
    const LzmwAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzmwAdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwAdaptiveHuffmanProfileError
calculate_lzmw_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzmwAdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] LzmwAdaptiveHuffmanWorkspaceError
partition_lzmw_adaptive_huffman_encoder_views(
    const LzmwAdaptiveHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwAdaptiveHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzmwAdaptiveHuffmanWorkspaceError
partition_lzmw_adaptive_huffman_decoder_views(
    const LzmwAdaptiveHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    LzmwAdaptiveHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lzmw_adaptive_huffman_profile_error_code(
    LzmwAdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

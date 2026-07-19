#ifndef MARC_FRAME_LZ78_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZ78_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "frame/lz78_adaptive_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

struct Lz78AdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::Lz78Parameters parameters{};
};

struct Lz78AdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t encoder_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct Lz78AdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t dictionary_staging_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t phrase_entry_count{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class Lz78AdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class Lz78AdaptiveHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct Lz78AdaptiveHuffmanEncoderViews {
    std::span<dictionary::internal::Lz78EncoderEntry> entries{};
};

struct Lz78AdaptiveHuffmanDecoderViews {
    std::span<dictionary::internal::Lz78PhraseEntry> phrases{};
};

[[nodiscard]] Lz78AdaptiveHuffmanProfileError
make_lz78_adaptive_huffman_profile(
    const Lz78AdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78AdaptiveHuffmanEncoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78AdaptiveHuffmanProfileError
calculate_lz78_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78AdaptiveHuffmanDecoderWorkspaceRequirements& workspace) noexcept;

[[nodiscard]] Lz78AdaptiveHuffmanWorkspaceError
partition_lz78_adaptive_huffman_encoder_views(
    const Lz78AdaptiveHuffmanEncoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78AdaptiveHuffmanEncoderViews& views) noexcept;

[[nodiscard]] Lz78AdaptiveHuffmanWorkspaceError
partition_lz78_adaptive_huffman_decoder_views(
    const Lz78AdaptiveHuffmanDecoderWorkspaceRequirements& requirements,
    std::span<std::byte> storage,
    Lz78AdaptiveHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode lz78_adaptive_huffman_profile_error_code(
    Lz78AdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame

#endif

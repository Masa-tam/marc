#ifndef MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_adaptive_huffman_model.hpp"
#include "frame/lzss_contextual_adaptive_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualAdaptiveHuffmanProfileVariant : std::uint8_t {
    field_context_64k,
    field_context_1m,
    field_context_4m,
};

struct LzssContextualAdaptiveHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters dictionary{};
    LzssContextualAdaptiveHuffmanProfileVariant variant{
        LzssContextualAdaptiveHuffmanProfileVariant::field_context_64k};
};

struct LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t token_count{};
    std::size_t node_count{};
    std::size_t node_offset{};
    std::size_t symbol_count{};
    std::size_t symbol_offset{};
    std::size_t match_finder_offset{};
    std::size_t match_finder_bytes{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t node_count{};
    std::size_t symbol_count{};
    std::size_t symbol_offset{};
    std::size_t token_count{};
    std::size_t token_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzssContextualAdaptiveHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzssContextualAdaptiveHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzssContextualAdaptiveHuffmanEncoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<entropy::internal::AdaptiveHuffmanNode> nodes{};
    std::span<std::uint16_t> symbols{};
    std::span<std::byte> match_finder{};
};

struct LzssContextualAdaptiveHuffmanDecoderViews {
    std::span<entropy::internal::AdaptiveHuffmanNode> nodes{};
    std::span<std::uint16_t> symbols{};
    std::span<dictionary::internal::LzssTypedToken> tokens{};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanProfileError
make_lzss_contextual_adaptive_huffman_profile(
    const LzssContextualAdaptiveHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanStreamHeader& stream,
    LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements& workspace)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanProfileError
calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements& workspace,
    LzssContextualAdaptiveHuffmanProfileVariant variant =
        LzssContextualAdaptiveHuffmanProfileVariant::field_context_64k)
    noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanWorkspaceError
partition_lzss_contextual_adaptive_huffman_encoder_views(
    const LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements&
        requirements,
    std::span<std::byte> storage,
    LzssContextualAdaptiveHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzssContextualAdaptiveHuffmanWorkspaceError
partition_lzss_contextual_adaptive_huffman_decoder_views(
    const LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements&
        requirements,
    std::span<std::byte> storage,
    LzssContextualAdaptiveHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode
lzss_contextual_adaptive_huffman_profile_error_code(
    LzssContextualAdaptiveHuffmanProfileError error) noexcept;

} // namespace marc::frame::internal

#endif

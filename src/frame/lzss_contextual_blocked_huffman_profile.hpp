#ifndef MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_PROFILE_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_PROFILE_HPP

#include "core/status.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/huffman_decode_table.hpp"
#include "frame/lzss_contextual_blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualBlockedHuffmanProfileVariant : std::uint8_t {
    field_context_64k,
    field_context_1m,
    field_context_4m,
    field_context_16m,
    field_context_64m,
};

struct LzssContextualBlockedHuffmanProfileConfig {
    std::uint64_t original_size{};
    std::uint32_t frame_size{UINT32_C(1) << 16};
    dictionary::internal::LzssParameters dictionary{};
    LzssContextualBlockedHuffmanProfileVariant variant{
        LzssContextualBlockedHuffmanProfileVariant::field_context_64k};
    dictionary::internal::LzssMatchFinderStrategy match_finder_strategy{
        dictionary::internal::LzssMatchFinderStrategy::hash_chain_exact};
};

struct LzssContextualBlockedHuffmanEncoderWorkspaceRequirements {
    std::size_t frame_input_bytes{};
    std::size_t frame_encoded_bytes{};
    std::size_t token_count{};
    std::size_t match_finder_offset{};
    std::size_t match_finder_bytes{};
    std::size_t match_finder_alignment{1};
    dictionary::internal::LzssMatchFinderStrategy match_finder_strategy{
        dictionary::internal::LzssMatchFinderStrategy::hash_chain_exact};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

struct LzssContextualBlockedHuffmanDecoderWorkspaceRequirements {
    std::size_t frame_encoded_bytes{};
    std::size_t frame_decoded_bytes{};
    std::size_t table_count{};
    std::size_t token_count{};
    std::size_t token_offset{};
    std::size_t views_bytes{};
    std::size_t views_alignment{1};
};

enum class LzssContextualBlockedHuffmanProfileError : std::uint8_t {
    none,
    invalid_configuration,
    unsupported,
    limit_exceeded,
    arithmetic_overflow,
};

enum class LzssContextualBlockedHuffmanWorkspaceError : std::uint8_t {
    none,
    invalid_requirements,
    too_small,
    misaligned,
    arithmetic_overflow,
};

struct LzssContextualBlockedHuffmanEncoderViews {
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<std::byte> match_finder{};
};

struct LzssContextualBlockedHuffmanDecoderViews {
    std::span<entropy::internal::HuffmanDecodeTable> tables{};
    std::span<dictionary::internal::LzssTypedToken> tokens{};
};

[[nodiscard]] LzssContextualBlockedHuffmanProfileError
make_lzss_contextual_blocked_huffman_profile(
    const LzssContextualBlockedHuffmanProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanStreamHeader& stream,
    LzssContextualBlockedHuffmanEncoderWorkspaceRequirements& workspace)
    noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanProfileError
calculate_lzss_contextual_blocked_huffman_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualBlockedHuffmanDecoderWorkspaceRequirements& workspace,
    LzssContextualBlockedHuffmanProfileVariant variant =
        LzssContextualBlockedHuffmanProfileVariant::field_context_64k)
    noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanWorkspaceError
partition_lzss_contextual_blocked_huffman_encoder_views(
    const LzssContextualBlockedHuffmanEncoderWorkspaceRequirements&
        requirements,
    std::span<std::byte> storage,
    LzssContextualBlockedHuffmanEncoderViews& views) noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanWorkspaceError
partition_lzss_contextual_blocked_huffman_decoder_views(
    const LzssContextualBlockedHuffmanDecoderWorkspaceRequirements&
        requirements,
    std::span<std::byte> storage,
    LzssContextualBlockedHuffmanDecoderViews& views) noexcept;

[[nodiscard]] core::ErrorCode
lzss_contextual_blocked_huffman_profile_error_code(
    LzssContextualBlockedHuffmanProfileError error) noexcept;

} // namespace marc::frame::internal

#endif

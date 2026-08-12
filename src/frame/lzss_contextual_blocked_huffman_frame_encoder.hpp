#ifndef MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FRAME_ENCODER_HPP

#include "context/lzss_contextual_blocked_huffman_encoder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualBlockedHuffmanFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    input_size_mismatch,
    token_staging_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    token_encode_error,
    entropy_encode_error,
    header_error,
    descriptor_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualBlockedHuffmanFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t descriptor_size{};
    std::size_t token_count{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssContextualBlockedHuffmanEncodeResult entropy_encode{};
    LzssContextualBlockedHuffmanFrameHeaderError header_error{
        LzssContextualBlockedHuffmanFrameHeaderError::none};
    entropy::internal::ContextualBlockedHuffmanFormatError descriptor_error{
        entropy::internal::ContextualBlockedHuffmanFormatError::none};
    LzssContextualBlockedHuffmanFrameEncodeError error{
        LzssContextualBlockedHuffmanFrameEncodeError::none};
};

[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult
plan_lzss_contextual_blocked_huffman_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult
encode_lzss_contextual_blocked_huffman_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> serialized_output) noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult
plan_lzss_contextual_blocked_huffman_frame_hash_chain(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult
encode_lzss_contextual_blocked_huffman_frame_hash_chain(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

} // namespace marc::frame::internal

#endif

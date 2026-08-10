#ifndef MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_BLOCKED_HUFFMAN_FRAME_DECODER_HPP

#include "context/lzss_contextual_blocked_huffman_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/lzss_contextual_blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualBlockedHuffmanFrameDecodeError : std::uint8_t {
    none,
    preflight_error,
    output_size_unsupported,
    table_output_too_small,
    token_output_too_small,
    raw_output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    token_decode_error,
    reconstruction_error,
};

struct LzssContextualBlockedHuffmanFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_table_entries{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    LzssContextualBlockedHuffmanFramePreflightResult preflight{};
    context::internal::LzssContextualBlockedHuffmanDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssContextualBlockedHuffmanFrameDecodeError error{
        LzssContextualBlockedHuffmanFrameDecodeError::none};
};

[[nodiscard]] LzssContextualBlockedHuffmanFrameDecodeResult
decode_lzss_contextual_blocked_huffman_frame(
    std::span<const std::byte> serialized_frame,
    const LzssContextualBlockedHuffmanFrameValidationContext& context,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif

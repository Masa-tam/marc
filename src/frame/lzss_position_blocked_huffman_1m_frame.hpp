#ifndef MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_1M_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_1M_FRAME_HPP
#include "frame/lzss_position_blocked_huffman_1m_format.hpp"
#include "context/lzss_position_blocked_huffman_1m_tokens.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionBlockedHuffman1mFrameError : std::uint8_t {
    none, format_error, token_error, reconstruction_error, output_too_small,
    overlapping_buffers, arithmetic_overflow, limit_exceeded
};
struct PositionBlockedHuffman1mFrameResult {
    // Committed counts stay zero on failure, including private scratch failure.
    std::size_t serialized_size{}, raw_size{};
    PositionBlockedHuffman1mFrameError error{};
    PositionBlockedHuffman1mFormatError format_error{};
    context::internal::PositionBlockedHuffman1mTokenResult token_result{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    PositionBlockedHuffman1mFrameRequirements requirements{};
};
// Caller-provided tables, payload and serialized scratch may change on failure.
// Ordinary serialized output (including its unused tail) commits only on success.
[[nodiscard]] PositionBlockedHuffman1mFrameResult encode_position_blocked_huffman_1m_frame(
    std::span<const dictionary::internal::LzssTypedToken>,
    const PositionBlockedHuffman1mFrameContext&, std::span<entropy::internal::CanonicalHuffmanTable> private_tables,
    std::span<std::byte> private_payload, std::span<std::byte> private_serialized,
    std::span<std::byte> serialized_output) noexcept;
// Single-pass entropy decoding into private tokens, then private raw restoration.
// Private scratch must be discarded on failure. Ordinary raw is success-only.
[[nodiscard]] PositionBlockedHuffman1mFrameResult decode_position_blocked_huffman_1m_frame(
    std::span<const std::byte>, const PositionBlockedHuffman1mFrameContext&,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw, std::span<std::byte> raw_output) noexcept;
// Controller entry: raw remains private until success and must never drain on error.
[[nodiscard]] PositionBlockedHuffman1mFrameResult decode_position_blocked_huffman_1m_frame_scratch(
    std::span<const std::byte>, const PositionBlockedHuffman1mFrameContext&,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw) noexcept;
} // namespace marc::frame::internal
#endif


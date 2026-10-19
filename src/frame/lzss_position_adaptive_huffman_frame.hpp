#ifndef MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_FRAME_HPP
#include "frame/lzss_position_adaptive_huffman_format.hpp"
#include "context/lzss_position_adaptive_huffman_tokens.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionAdaptiveHuffmanFrameError : std::uint8_t {
    none,format_error,token_error,reconstruction_error,output_too_small,
    overlapping_buffers,arithmetic_overflow,limit_exceeded
};
struct PositionAdaptiveHuffmanFrameResult {
    // Committed sizes and nested token counts remain zero on every failure.
    std::size_t serialized_size{},raw_size{};
    PositionAdaptiveHuffmanFrameError error{};
    PositionAdaptiveHuffmanFormatError format_error{};
    context::internal::PositionAdaptiveHuffmanTokenResult token_result{};
    dictionary::internal::LzssTypedReconstructError reconstruction_error{};
    PositionAdaptiveHuffmanFrameRequirements requirements{};
};
// All ordinary serialized bytes/tails commit only after token writing and
// header validation succeed. Private model/payload/serialized staging is
// discard-on-error. The reference performs one FGK plan and one FGK write.
[[nodiscard]] PositionAdaptiveHuffmanFrameResult encode_position_adaptive_huffman_frame(
    std::span<const dictionary::internal::LzssTypedToken>,const PositionAdaptiveHuffmanFrameContext&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<std::byte> private_payload,std::span<std::byte> private_serialized,
    std::span<std::byte> serialized_output) noexcept;
// Decode once to private tokens; validate and restore private raw before any
// ordinary raw output is copied. A failed frame publishes no raw bytes.
[[nodiscard]] PositionAdaptiveHuffmanFrameResult decode_position_adaptive_huffman_frame(
    std::span<const std::byte>,const PositionAdaptiveHuffmanFrameContext&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw,std::span<std::byte> raw_output) noexcept;
// Controller entry: raw remains private; never drain it unless this succeeds.
[[nodiscard]] PositionAdaptiveHuffmanFrameResult decode_position_adaptive_huffman_frame_scratch(
    std::span<const std::byte>,const PositionAdaptiveHuffmanFrameContext&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,std::span<std::byte> private_raw) noexcept;
} // namespace marc::frame::internal
#endif

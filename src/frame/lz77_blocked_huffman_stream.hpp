#ifndef MARC_FRAME_LZ77_BLOCKED_HUFFMAN_STREAM_HPP
#define MARC_FRAME_LZ77_BLOCKED_HUFFMAN_STREAM_HPP

#include "frame/lz77_blocked_huffman_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz77BlockedHuffmanStreamCodecError : std::uint8_t {
    none,
    invalid_stream_header,
    invalid_parameters,
    unsupported_pipeline,
    input_size_mismatch,
    output_too_small,
    view_output_too_small,
    dictionary_staging_too_small,
    truncated_stream,
    trailing_stream_bytes,
    frame_error,
    arithmetic_overflow,
    internal_error,
};

struct Lz77BlockedHuffmanStreamCodecResult {
    std::size_t serialized_size{};
    std::size_t output_size{};
    std::size_t frame_count{};
    std::size_t frame_index{};
    StreamHeaderError stream_header_error{StreamHeaderError::none};
    dictionary::internal::Lz77FormatError parameter_error{
        dictionary::internal::Lz77FormatError::none};
    Lz77BlockedHuffmanFrameValidationError frame_error{
        Lz77BlockedHuffmanFrameValidationError::none};
    Lz77BlockedHuffmanStreamCodecError error{
        Lz77BlockedHuffmanStreamCodecError::none};
};

// Input and dictionary staging must not overlap. Planning may replace staging
// contents while determining exact frame sizes.
[[nodiscard]] Lz77BlockedHuffmanStreamCodecResult
plan_lz77_blocked_huffman_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Input, dictionary staging, and serialized output must be mutually
// non-overlapping.
[[nodiscard]] Lz77BlockedHuffmanStreamCodecResult
encode_lz77_blocked_huffman_stream(
    const StreamHeader& stream,
    const dictionary::internal::Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Serialized input, views, dictionary staging, and raw output must occupy
// disjoint storage. Configuration outputs are published only on success.
[[nodiscard]] Lz77BlockedHuffmanStreamCodecResult
decode_lz77_blocked_huffman_stream(
    std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::span<entropy::internal::BlockedHuffmanBlockView> frame_views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output,
    StreamHeader& stream,
    dictionary::internal::Lz77Parameters& parameters) noexcept;

} // namespace marc::frame

#endif

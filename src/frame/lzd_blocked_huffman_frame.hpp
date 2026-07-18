#ifndef MARC_FRAME_LZD_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZD_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lzd_decoder.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzdBlockedHuffmanFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    view_output_too_small,
    dictionary_staging_too_small,
    phrase_workspace_too_small,
    expansion_workspace_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    raw_output_too_small,
    dictionary_decode_error,
    arithmetic_overflow,
};

struct LzdBlockedHuffmanFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t block_count{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t token_count{};
    std::size_t phrase_entries{};
    std::size_t dictionary_entries{};
    std::size_t expansion_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::BlockedHuffmanControllerError controller_error{
        entropy::internal::BlockedHuffmanControllerError::none};
    entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
        entropy::internal::BlockedHuffmanFrameDecodeError::none};
    dictionary::internal::LzdValidationError dictionary_error{
        dictionary::internal::LzdValidationError::none};
    dictionary::internal::LzdFormatError dictionary_format_error{
        dictionary::internal::LzdFormatError::none};
    dictionary::internal::LzdDecodeError dictionary_decode_error{
        dictionary::internal::LzdDecodeError::none};
    LzdBlockedHuffmanFrameValidationError error{
        LzdBlockedHuffmanFrameValidationError::none};
};

[[nodiscard]] LzdBlockedHuffmanFrameValidationResult
validate_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace) noexcept;

[[nodiscard]] LzdBlockedHuffmanFrameValidationResult
decode_lzd_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzdParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

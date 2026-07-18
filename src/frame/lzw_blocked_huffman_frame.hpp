#ifndef MARC_FRAME_LZW_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZW_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_validator.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwBlockedHuffmanFrameValidationError : std::uint8_t {
  none,
  unsupported_pipeline,
  truncated_frame,
  trailing_frame_bytes,
  header_error,
  view_output_too_small,
  dictionary_staging_too_small,
  phrase_workspace_too_small,
  workspace_limit,
  controller_error,
  entropy_decode_error,
  dictionary_validation_error,
  raw_output_too_small,
  dictionary_decode_error,
  arithmetic_overflow,
};

struct LzwBlockedHuffmanFrameValidationResult {
  std::size_t serialized_size{};
  std::size_t dictionary_size{};
  std::size_t raw_size{};
  std::size_t block_count{};
  std::size_t descriptor_size{};
  std::size_t payload_size{};
  std::size_t phrase_entries{};
  FrameHeaderError header_error{FrameHeaderError::none};
  entropy::internal::BlockedHuffmanControllerError controller_error{
      entropy::internal::BlockedHuffmanControllerError::none};
  entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
      entropy::internal::BlockedHuffmanFrameDecodeError::none};
  dictionary::internal::LzwValidationError dictionary_error{
      dictionary::internal::LzwValidationError::none};
  dictionary::internal::LzwFormatError dictionary_format_error{
      dictionary::internal::LzwFormatError::none};
  dictionary::internal::LzwDecodeError dictionary_decode_error{
      dictionary::internal::LzwDecodeError::none};
  LzwBlockedHuffmanFrameValidationError error{
      LzwBlockedHuffmanFrameValidationError::none};
};

// Entropy-decodes and validates one complete frame without exposing raw bytes.
// Block views and LZW phrase entries are distinct typed views of the opaque
// workspace that a future public adapter will partition.
[[nodiscard]] LzwBlockedHuffmanFrameValidationResult
validate_lzw_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::LzwParameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry> phrase_workspace) noexcept;

// Validates both layers before publishing raw bytes. Input, dictionary staging,
// and output must not overlap.
[[nodiscard]] LzwBlockedHuffmanFrameValidationResult
decode_lzw_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::LzwParameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzwPhraseEntry> phrase_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

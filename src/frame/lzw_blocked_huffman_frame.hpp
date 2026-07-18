#ifndef MARC_FRAME_LZW_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZW_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_encoder.hpp"
#include "dictionary/lzw_validator.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzwBlockedHuffmanFrameValidationError : std::uint8_t {
  none,
  unsupported_pipeline,
  input_size_mismatch,
  serialized_output_too_small,
  truncated_frame,
  trailing_frame_bytes,
  header_error,
  view_output_too_small,
  dictionary_staging_too_small,
  encoder_workspace_too_small,
  phrase_workspace_too_small,
  workspace_limit,
  controller_error,
  entropy_decode_error,
  dictionary_validation_error,
  raw_output_too_small,
  dictionary_decode_error,
  dictionary_encode_error,
  entropy_encode_error,
  arithmetic_overflow,
  internal_error,
};

struct LzwBlockedHuffmanFrameValidationResult {
  std::size_t serialized_size{};
  std::size_t dictionary_size{};
  std::size_t raw_size{};
  std::size_t block_count{};
  std::size_t descriptor_size{};
  std::size_t payload_size{};
  std::size_t phrase_entries{};
  std::size_t encoder_entries{};
  std::size_t code_count{};
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
  dictionary::internal::LzwEncodeError dictionary_encode_error{
      dictionary::internal::LzwEncodeError::none};
  entropy::internal::BlockedHuffmanFrameEncodeError entropy_encode_error{
      entropy::internal::BlockedHuffmanFrameEncodeError::none};
  LzwBlockedHuffmanFrameValidationError error{
      LzwBlockedHuffmanFrameValidationError::none};
};

// Fixes the complete packed LZW code stream, including its final zero padding,
// in dictionary_staging before planning Blocked Huffman over those exact bytes.
// Raw input and staging must not overlap.
[[nodiscard]] LzwBlockedHuffmanFrameValidationResult
plan_lzw_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::LzwParameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, dictionary
// staging, and serialized output must be mutually non-overlapping.
[[nodiscard]] LzwBlockedHuffmanFrameValidationResult
encode_lzw_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::LzwParameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<dictionary::internal::LzwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

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

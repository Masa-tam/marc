#ifndef MARC_FRAME_LZ78_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZ78_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_validator.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class Lz78BlockedHuffmanFrameValidationError : std::uint8_t {
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

struct Lz78BlockedHuffmanFrameValidationResult {
  std::size_t serialized_size{};
  std::size_t dictionary_size{};
  std::size_t raw_size{};
  std::size_t block_count{};
  std::size_t descriptor_size{};
  std::size_t payload_size{};
  std::size_t phrase_entries{};
  std::size_t encoder_entries{};
  FrameHeaderError header_error{FrameHeaderError::none};
  entropy::internal::BlockedHuffmanControllerError controller_error{
      entropy::internal::BlockedHuffmanControllerError::none};
  entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
      entropy::internal::BlockedHuffmanFrameDecodeError::none};
  dictionary::internal::Lz78ValidationError dictionary_error{
      dictionary::internal::Lz78ValidationError::none};
  dictionary::internal::Lz78FormatError dictionary_format_error{
      dictionary::internal::Lz78FormatError::none};
  dictionary::internal::Lz78DecodeError dictionary_decode_error{
      dictionary::internal::Lz78DecodeError::none};
  dictionary::internal::Lz78EncodeError dictionary_encode_error{
      dictionary::internal::Lz78EncodeError::none};
  entropy::internal::BlockedHuffmanFrameEncodeError entropy_encode_error{
      entropy::internal::BlockedHuffmanFrameEncodeError::none};
  Lz78BlockedHuffmanFrameValidationError error{
      Lz78BlockedHuffmanFrameValidationError::none};
};

// Fixes the complete LZ78 parse in dictionary_staging, then plans Blocked
// Huffman over those exact bytes. Raw input and staging must not overlap.
[[nodiscard]] Lz78BlockedHuffmanFrameValidationResult
plan_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, dictionary
// staging, and serialized output must be mutually non-overlapping.
[[nodiscard]] Lz78BlockedHuffmanFrameValidationResult
encode_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<dictionary::internal::Lz78EncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

// Entropy-decodes and validates one complete frame without exposing raw bytes.
// The caller-owned block views and phrase entries are distinct typed views of
// the opaque workspace that a future public adapter will partition.
[[nodiscard]] Lz78BlockedHuffmanFrameValidationResult
validate_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace) noexcept;

// Validates the complete entropy and dictionary layers before publishing raw
// bytes. Input, dictionary staging, and output must not overlap.
[[nodiscard]] Lz78BlockedHuffmanFrameValidationResult
decode_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed, std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

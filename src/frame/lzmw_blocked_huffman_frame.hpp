#ifndef MARC_FRAME_LZMW_BLOCKED_HUFFMAN_FRAME_HPP
#define MARC_FRAME_LZMW_BLOCKED_HUFFMAN_FRAME_HPP

#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_encoder.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "entropy/blocked_huffman_frame_decoder.hpp"
#include "entropy/blocked_huffman_frame_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzmwBlockedHuffmanFrameValidationError : std::uint8_t {
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
    serialized_output_too_small,
    input_size_mismatch,
    encoder_workspace_too_small,
    dictionary_encode_error,
    entropy_encode_error,
    internal_error,
};

struct LzmwBlockedHuffmanFrameValidationResult {
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
    std::size_t encoder_entries{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::BlockedHuffmanControllerError controller_error{
        entropy::internal::BlockedHuffmanControllerError::none};
    entropy::internal::BlockedHuffmanFrameDecodeError entropy_error{
        entropy::internal::BlockedHuffmanFrameDecodeError::none};
    dictionary::internal::LzmwValidationError dictionary_error{
        dictionary::internal::LzmwValidationError::none};
    dictionary::internal::LzmwFormatError dictionary_format_error{
        dictionary::internal::LzmwFormatError::none};
    dictionary::internal::LzmwDecodeError dictionary_decode_error{
        dictionary::internal::LzmwDecodeError::none};
    dictionary::internal::LzmwEncodeError dictionary_encode_error{
        dictionary::internal::LzmwEncodeError::none};
    entropy::internal::BlockedHuffmanFrameEncodeError entropy_encode_error{
        entropy::internal::BlockedHuffmanFrameEncodeError::none};
    LzmwBlockedHuffmanFrameValidationError error{
        LzmwBlockedHuffmanFrameValidationError::none};
};

// Fixes the complete canonical LZMW reference stream in dictionary_staging
// before planning Blocked Huffman over those exact bytes. Raw input and staging
// must not overlap.
[[nodiscard]] LzmwBlockedHuffmanFrameValidationResult
plan_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging) noexcept;

// Plans completely before writing serialized output. Input, dictionary staging,
// and serialized output must be mutually non-overlapping.
[[nodiscard]] LzmwBlockedHuffmanFrameValidationResult
encode_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<dictionary::internal::LzmwEncoderEntry> encoder_workspace,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> output) noexcept;

[[nodiscard]] LzmwBlockedHuffmanFrameValidationResult
validate_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry>
        phrase_workspace) noexcept;

[[nodiscard]] LzmwBlockedHuffmanFrameValidationResult
decode_lzmw_blocked_huffman_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzmwParameters& parameters,
    const core::DecoderLimits& limits, std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::BlockedHuffmanBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

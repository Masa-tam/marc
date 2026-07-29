#ifndef MARC_FRAME_LZSS_RANS_FRAME_HPP
#define MARC_FRAME_LZSS_RANS_FRAME_HPP

#include "dictionary/lzss_decoder.hpp"
#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_validator.hpp"
#include "entropy/rans_controller.hpp"
#include "entropy/rans_decoder.hpp"
#include "entropy/rans_encoder.hpp"
#include "frame/frame_header.hpp"
#include "frame/stream_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

enum class LzssRansFrameValidationError : std::uint8_t {
    none,
    unsupported_pipeline,
    input_size_mismatch,
    truncated_frame,
    trailing_frame_bytes,
    header_error,
    invalid_dictionary_extent,
    invalid_entropy_extent,
    views_too_small,
    dictionary_staging_too_small,
    raw_staging_too_small,
    raw_output_too_small,
    workspace_limit,
    controller_error,
    entropy_decode_error,
    dictionary_validation_error,
    dictionary_decode_error,
    dictionary_encode_error,
    entropy_encode_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssRansFrameValidationResult {
    std::size_t serialized_size{};
    std::size_t dictionary_size{};
    std::size_t raw_size{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    std::size_t block_count{};
    std::size_t block_index{};
    std::size_t dictionary_token_index{};
    std::size_t dictionary_input_offset{};
    FrameHeaderError header_error{FrameHeaderError::none};
    entropy::internal::RansControllerError controller_error{
        entropy::internal::RansControllerError::none};
    entropy::internal::RansDecodeError entropy_error{
        entropy::internal::RansDecodeError::none};
    dictionary::internal::LzssValidationError dictionary_error{
        dictionary::internal::LzssValidationError::none};
    dictionary::internal::LzssFormatError dictionary_format_error{
        dictionary::internal::LzssFormatError::none};
    dictionary::internal::LzssDecodeError dictionary_decode_error{
        dictionary::internal::LzssDecodeError::none};
    dictionary::internal::LzssEncodeError dictionary_encode_error{
        dictionary::internal::LzssEncodeError::none};
    entropy::internal::RansEncodeError entropy_encode_error{
        entropy::internal::RansEncodeError::none};
    LzssRansFrameValidationError error{
        LzssRansFrameValidationError::none};
};

// Produces canonical LZSS staging and determines every rANS block and the
// complete frame extent without writing serialized output. Input and staging
// must not overlap.
[[nodiscard]] LzssRansFrameValidationResult plan_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates and rANS-decodes one complete frame into private canonical LZSS
// token staging. Every rANS block is validated before any token byte is
// written. The complete LZSS grammar is then validated without reconstructing
// or publishing raw bytes. Input, views, and staging must not overlap.
[[nodiscard]] LzssRansFrameValidationResult validate_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging) noexcept;

// Validates every layer and reconstructs exactly one frame into caller-owned
// private raw staging. No caller-visible output extent is published. Input,
// views, token staging, and raw staging must not overlap.
[[nodiscard]] LzssRansFrameValidationResult
decode_lzss_rans_frame_to_staging(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging) noexcept;

// Validates every layer, reconstructs into private raw staging, and publishes
// to output only after reconstruction succeeds. Input, views, both staging
// extents, and output must be mutually non-overlapping.
[[nodiscard]] LzssRansFrameValidationResult decode_lzss_rans_frame(
    const StreamHeader& stream,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint64_t expected_sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> input,
    std::span<entropy::internal::RansBlockView> views,
    std::span<std::byte> dictionary_staging,
    std::span<std::byte> raw_staging,
    std::span<std::byte> output) noexcept;

} // namespace marc::frame

#endif

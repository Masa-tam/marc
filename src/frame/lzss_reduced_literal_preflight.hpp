#ifndef MARC_FRAME_LZSS_REDUCED_LITERAL_PREFLIGHT_HPP
#define MARC_FRAME_LZSS_REDUCED_LITERAL_PREFLIGHT_HPP

#include "frame/lzss_short_match_preflight.hpp"

namespace marc::frame::internal {

// Private checks only; no public format admission.
[[nodiscard]] LzssShortMatchPreflightError validate_lzss_reduced_literal_stream_semantics(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits) noexcept;

// Requirements remain unchanged on failure. Success validates sizes, not payload.
[[nodiscard]] LzssShortMatchPreflightError preflight_lzss_reduced_literal_frame_semantics(
    const TypedContextFrameHeader& frame, const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept;

// Parse one header/frame prefix. Outputs remain unchanged on failure.
// Success reports the exact extent; subsequent bytes belong to the caller.
// Frame success does not validate the Range payload or publish raw bytes.
[[nodiscard]] LzssShortMatchPreflightError parse_lzss_reduced_literal_stream_header(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream, std::size_t& bytes_consumed) noexcept;

[[nodiscard]] LzssShortMatchPreflightError preflight_lzss_reduced_literal_frame_bytes(
    std::span<const std::byte> input, const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout, LzssShortMatchFrameRequirements& requirements) noexcept;

} // namespace marc::frame::internal
#endif

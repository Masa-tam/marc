#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_PREFLIGHT_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_PREFLIGHT_HPP

#include "frame/lzss_short_match_preflight.hpp"

namespace marc::frame::internal {

// Private 2/8 + 1/7 + 3/2 semantic and byte preflight. The published
// stream parser continues to reject this identity.
[[nodiscard]] LzssShortMatchPreflightError
validate_lzss_short_length_escape_stream_semantics(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssShortMatchPreflightError
preflight_lzss_short_length_escape_frame_semantics(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept;

[[nodiscard]] LzssShortMatchPreflightError
parse_lzss_short_length_escape_stream_header(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream, std::size_t& bytes_consumed) noexcept;

[[nodiscard]] LzssShortMatchPreflightError
preflight_lzss_short_length_escape_frame_bytes(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext& context,
    TypedContextFrameLayout& layout,
    LzssShortMatchFrameRequirements& requirements) noexcept;

} // namespace marc::frame::internal

#endif

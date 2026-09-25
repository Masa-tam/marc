#ifndef MARC_FRAME_LZSS_REDUCED_LITERAL_PREFLIGHT_HPP
#define MARC_FRAME_LZSS_REDUCED_LITERAL_PREFLIGHT_HPP

#include "frame/lzss_short_match_preflight.hpp"

namespace marc::frame::internal {

// Private semantic checks only, not byte parsing or public format admission.
[[nodiscard]] LzssShortMatchPreflightError validate_lzss_reduced_literal_stream_semantics(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits) noexcept;

// Requirements remain unchanged on failure. Success validates sizes, not payload.
[[nodiscard]] LzssShortMatchPreflightError preflight_lzss_reduced_literal_frame_semantics(
    const TypedContextFrameHeader& frame, const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept;

} // namespace marc::frame::internal
#endif

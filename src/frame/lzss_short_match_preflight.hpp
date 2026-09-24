#ifndef MARC_FRAME_LZSS_SHORT_MATCH_PREFLIGHT_HPP
#define MARC_FRAME_LZSS_SHORT_MATCH_PREFLIGHT_HPP

#include "frame/typed_context_format.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::frame::internal {

// Private semantic preflight for the reserved 2/7 + 1/6 + 3/2 identity.
// This does not parse bytes or open the public stream admission gate.
enum class LzssShortMatchPreflightError : std::uint8_t {
    none,
    invalid_stream,
    unexpected_sequence,
    unexpected_frame_size,
    contradictory_counts,
    invalid_descriptor,
    unsupported_feature,
    limit_exceeded,
    arithmetic_overflow,
};

struct LzssShortMatchFrameRequirements {
    std::size_t serialized_frame_bytes{};
    std::size_t token_count{};
    std::size_t raw_frame_bytes{};
    std::size_t aggregate_working_bytes{};
};

[[nodiscard]] LzssShortMatchPreflightError
validate_lzss_short_match_stream_semantics(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssShortMatchPreflightError
preflight_lzss_short_match_frame_semantics(
    const TypedContextFrameHeader& frame,
    const TypedContextRangeDescriptor& descriptor,
    const TypedContextFrameValidationContext& context,
    LzssShortMatchFrameRequirements& requirements) noexcept;

} // namespace marc::frame::internal

#endif

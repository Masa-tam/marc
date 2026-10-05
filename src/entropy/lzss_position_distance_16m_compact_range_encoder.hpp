#ifndef MARC_ENTROPY_POSITION_DISTANCE16M_COMPACT_RANGE_HPP
#define MARC_ENTROPY_POSITION_DISTANCE16M_COMPACT_RANGE_HPP
#include "dictionary/lzss_position_distance_16m_compact.hpp"
#include "entropy/lzss_position_distance_16m_token_range_encoder.hpp"
namespace marc::entropy::internal {
[[nodiscard]] std::size_t
lzss_position_distance_16m_compact_range_working_bytes() noexcept;
// Entire canonical byte view validated before Range finish. Includes generic
// lengths3/4 and distinct sixteen-MiB history/raw/token/event/decision counts.
// FULL input-view + BOTH payload capacities + controls + retained owners.
// Any unused backing input capacity belongs in retained_owner_bytes.
[[nodiscard]] LzssPositionDistance16mTokenRangePlan
query_lzss_position_distance_16m_compact_range_encode(
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const context::internal::LzssFieldContextValidationContext &,
    const core::DecoderLimits &, std::size_t output_capacity,
    std::size_t scratch_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
// ANY failure preserves WHOLE caller output and descriptor, committed0.
// All full regions/config/descriptor disjoint, immutable bytes stable across
// count/write. Private payload scratch discardable; no native token casts.
[[nodiscard]] LzssPositionDistance16mTokenRangeResult
encode_lzss_position_distance_16m_compact_range(
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const context::internal::LzssFieldContextValidationContext &,
    const core::DecoderLimits &, std::span<std::byte> output,
    std::span<std::byte> scratch, ContextualDynamicRangeDescriptor &,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::entropy::internal
#endif

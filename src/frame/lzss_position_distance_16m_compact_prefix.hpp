#ifndef MARC_FRAME_POSITION_DISTANCE16M_COMPACT_PREFIX_HPP
#define MARC_FRAME_POSITION_DISTANCE16M_COMPACT_PREFIX_HPP
#include "frame/lzss_position_distance_16m_serializer.hpp"
namespace marc::frame::internal {
// Same wire bounds and fields as the typed prefix parser. This separate private
// encoder path accounts actual compact storage instead of a typed-token owner.
// Prefix success never proves payload validity or admits a public decoder.
[[nodiscard]] LzssPositionDistance16mPreflightError
preflight_lzss_position_distance_16m_compact_prefix(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    TypedContextFrameLayout &, LzssPositionDistance16mFrameRequirements &,
    std::size_t compact_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
[[nodiscard]] std::size_t
lzss_position_distance_16m_compact_prefix_working_bytes() noexcept;
[[nodiscard]] LzssPositionDistance16mSerializePlan
query_lzss_position_distance_16m_compact_prefix_serialize(
    const TypedContextFrameLayout &, const TypedContextFrameValidationContext &,
    std::size_t output_capacity, std::size_t compact_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
// Whole caller output and written count unchanged on ANY failure.
// Full regions/config/metadata disjoint; stable borrowed values.
[[nodiscard]] LzssPositionDistance16mSerializeResult
serialize_lzss_position_distance_16m_compact_prefix(
    const TypedContextFrameLayout &, const TypedContextFrameValidationContext &,
    std::span<std::byte>, std::size_t &written, std::size_t compact_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

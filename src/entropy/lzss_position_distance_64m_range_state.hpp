#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_64M_RANGE_STATE_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_64M_RANGE_STATE_HPP
#include "context/lzss_position_distance_64m_context_layout.hpp"
#include "entropy/contextual_dynamic_range_decoder.hpp"
namespace marc::entropy::internal {
// Bounded storage charged by private preflight and used by the private decoder.
// The operation decoder statically asserts sizeof agreement with this type.
struct LzssPositionDistance64mRangeState {
  std::array<std::uint16_t, 2632> frequencies{};
  std::array<std::uint32_t, 50> totals{};
  context::internal::LzssPositionDistance64mFieldState cursor{};
  std::span<const std::byte> payload{};
  ContextualDynamicRangeDescriptor descriptor{};
  std::size_t payload_offset{};
  std::uint32_t code{}, range{UINT32_MAX}, event_count{}, decision_count{};
  std::uint64_t canonical_low{};
  std::size_t canonical_pending{1}, canonical_offset{};
  std::uint8_t canonical_cache{};
  bool canonical_mismatch{};
  ContextualDynamicRangeDecodeError error{
      ContextualDynamicRangeDecodeError::not_started};
  bool started{}, finished{};
};
} // namespace marc::entropy::internal
#endif

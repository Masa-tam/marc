#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_STATE_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_STATE_HPP

#include "context/lzss_position_distance_context_layout.hpp"
#include "context/lzss_position_distance_field_cursor.hpp"
#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

// Concrete private decoder storage, including canonical interval replay.
// Preflight charges sizeof including padding, counters and payload view.
// This is not a native serialized structure.
struct LzssPositionDistanceRangeState {
    std::array<std::uint16_t, context::internal::lzss_position_distance_frequency_entries> frequencies{};
    std::array<std::uint32_t, context::internal::lzss_position_distance_context_count> totals{};
    context::internal::LzssPositionDistanceFieldCursor cursor{};
    std::span<const std::byte> payload{};
    ContextualDynamicRangeDescriptor descriptor{};
    std::size_t payload_offset{};
    std::uint32_t code{};
    std::uint32_t range{UINT32_MAX};
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::uint64_t canonical_low{};
    std::size_t canonical_pending{1};
    std::size_t canonical_offset{};
    std::uint8_t canonical_cache{};
    bool canonical_mismatch{};
    ContextualDynamicRangeDecodeError error{ContextualDynamicRangeDecodeError::not_started};
    bool started{};
    bool finished{};
};

} // namespace marc::entropy::internal
#endif

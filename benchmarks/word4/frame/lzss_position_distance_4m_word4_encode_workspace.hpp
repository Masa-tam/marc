#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_WORD4_ENCODE_WORKSPACE_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_WORD4_ENCODE_WORKSPACE_HPP

#include "frame/lzss_position_distance_4m_preflight.hpp"
#include "dictionary/lzss_position_distance_4m_word4_candidate.hpp"
#include "frame/lzss_position_distance_workspace.hpp"

namespace marc::frame::internal {

// Private layout, not a public ABI. The owner supplies sizeof(its transform)
// as stream_state_bytes. F is the configured frame size, including empty input.
// Encoder storage includes the fixed exact five-prefix finder.
// Outputs remain unchanged on failure. Sizes are policy charges, not peak RSS.
[[nodiscard]] LzssPositionDistanceWorkspaceError
calculate_lzss_position_distance_4m_word4_encode_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    LzssPositionDistanceWorkspaceDirection direction, std::size_t stream_state_bytes,
    LzssPositionDistanceWorkspaceRequirements& requirements) noexcept;

// Recompute the layout rather than trusting caller-modifiable offsets. All
// supplied capacity is charged; returned views expose only the required extent.
// Storage must be writable, disjoint and live for the full owner lifetime.
[[nodiscard]] LzssPositionDistanceWorkspaceError
partition_lzss_position_distance_4m_word4_encode_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    LzssPositionDistanceWorkspaceDirection direction, std::size_t stream_state_bytes,
    std::span<std::byte> raw, std::span<std::byte> serialized,
    std::span<std::byte> storage, LzssPositionDistanceWorkspaceViews& views) noexcept;

// Encoding only: decode direction is rejected. Includes full validation state.
[[nodiscard]] LzssPositionDistanceWorkspaceError
charge_lzss_position_distance_4m_word4_encode_workspace(
    const core::DecoderLimits& limits, LzssPositionDistanceWorkspaceDirection direction,
    std::size_t stream_state_bytes, std::size_t raw_bytes,
    std::size_t serialized_bytes, std::size_t views_bytes,
    std::size_t& aggregate_bytes) noexcept;

} // namespace marc::frame::internal
#endif

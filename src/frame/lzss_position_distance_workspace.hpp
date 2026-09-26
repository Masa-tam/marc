#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_WORKSPACE_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_WORKSPACE_HPP

#include "frame/lzss_position_distance_preflight.hpp"
#include "context/lzss_field_context.hpp"

namespace marc::frame::internal {

enum class LzssPositionDistanceWorkspaceDirection { encode, decode };
enum class LzssPositionDistanceWorkspaceError {
    none, invalid_configuration, limit_exceeded, arithmetic_overflow,
    too_small, misaligned, overlapping_buffers
};
struct LzssPositionDistanceWorkspaceRequirements {
    std::size_t raw_bytes{}, serialized_bytes{}, token_count{}, operation_count{};
    std::size_t operation_offset{}, finder_offset{}, finder_bytes{};
    std::size_t views_bytes{}, views_alignment{1};
    std::size_t model_state_bytes{}, stream_state_bytes{}, aggregate_bytes{};
    bool operator==(const LzssPositionDistanceWorkspaceRequirements&) const = default;
};
struct LzssPositionDistanceWorkspaceViews {
    std::span<std::byte> raw{}, serialized{}, finder{};
    std::span<dictionary::internal::LzssTypedToken> tokens{};
    std::span<context::internal::ModeledOperation> operations{};
};

// Private layout, not a public ABI. The owner supplies sizeof(its transform)
// as stream_state_bytes. F is the configured frame size, including empty input.
// Encoder storage includes the exact indexed finder even for reference tests.
// Outputs remain unchanged on failure. Sizes are policy charges, not peak RSS.
[[nodiscard]] LzssPositionDistanceWorkspaceError
calculate_lzss_position_distance_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    LzssPositionDistanceWorkspaceDirection direction, std::size_t stream_state_bytes,
    LzssPositionDistanceWorkspaceRequirements& requirements) noexcept;

// Recompute the layout rather than trusting caller-modifiable offsets. All
// supplied capacity is charged; returned views expose only the required extent.
// Storage must be writable, disjoint and live for the full owner lifetime.
[[nodiscard]] LzssPositionDistanceWorkspaceError
partition_lzss_position_distance_workspace(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    LzssPositionDistanceWorkspaceDirection direction, std::size_t stream_state_bytes,
    std::span<std::byte> raw, std::span<std::byte> serialized,
    std::span<std::byte> storage, LzssPositionDistanceWorkspaceViews& views) noexcept;

// Shared by query, partition and decoder construction. Includes the larger
// encoder/validation model state on encode, the concrete replay state on decode.
[[nodiscard]] LzssPositionDistanceWorkspaceError
charge_lzss_position_distance_workspace(
    const core::DecoderLimits& limits, LzssPositionDistanceWorkspaceDirection direction,
    std::size_t stream_state_bytes, std::size_t raw_bytes,
    std::size_t serialized_bytes, std::size_t views_bytes,
    std::size_t& aggregate_bytes) noexcept;

} // namespace marc::frame::internal
#endif

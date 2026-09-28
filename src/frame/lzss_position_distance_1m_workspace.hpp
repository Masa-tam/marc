#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_WORKSPACE_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_WORKSPACE_HPP
#include "core/limits.hpp"
#include "core/status.hpp"

namespace marc::frame::internal {
struct LzssPositionDistance1mDecodeWorkspace {
    std::size_t raw_bytes{}, serialized_bytes{}, token_count{}, token_bytes{};
    std::size_t retained_state_bytes{}, model_bytes{}, aggregate_bytes{};
    bool operator==(const LzssPositionDistance1mDecodeWorkspace&) const = default;
};
// Failure preserves output. All supplied capacities, not merely used prefixes,
// are charged with retained owner/stream state and the complete replay decoder.
[[nodiscard]] core::ErrorCode charge_lzss_position_distance_1m_decode_workspace(
    const core::DecoderLimits& limits, std::size_t retained_state_bytes,
    std::size_t raw_bytes, std::size_t serialized_bytes, std::size_t token_bytes,
    std::size_t& aggregate_bytes) noexcept;
[[nodiscard]] core::ErrorCode calculate_lzss_position_distance_1m_decode_workspace(
    std::uint32_t frame_capacity, const core::DecoderLimits& limits,
    std::size_t retained_state_bytes, LzssPositionDistance1mDecodeWorkspace& result) noexcept;
}
#endif

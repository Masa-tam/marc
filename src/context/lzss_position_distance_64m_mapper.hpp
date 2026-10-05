#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_MAPPER_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_MAPPER_HPP
#include "context/lzss_position_distance_64m_tokens.hpp"
namespace marc::context::internal {
struct LzssPositionDistance64mMapPlan {
  LzssFieldContextResult details{};
  std::size_t aggregate_bytes{}, working_state_bytes{};
  LzssFieldContextError error{LzssFieldContextError::none};
};
struct LzssPositionDistance64mMapResult {
  LzssFieldContextResult details{};
  std::size_t operations_committed{};
};
[[nodiscard]] std::size_t
lzss_position_distance_64m_map_working_bytes() noexcept;
// Validates exact declared T/E/D/F and frame-local history without writing.
// Charges token view, BOTH complete operation capacities and concrete state.
// Retained bytes include input-owner capacity beyond the logical token view.
[[nodiscard]] LzssPositionDistance64mMapPlan
query_lzss_position_distance_64m_map(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::size_t operation_capacity, std::size_t scratch_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
// Caller operations AND metadata unchanged on every failure. Private scratch
// may retain a discardable prefix. Full spans/config/metadata are disjoint;
// borrowed inputs remain stable. No allocation or public encoder admission.
[[nodiscard]] LzssPositionDistance64mMapResult
map_lzss_position_distance_64m_tokens(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::span<ModeledOperation> output,
    std::span<ModeledOperation> private_scratch,
    LzssFieldContextResult &metadata,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::context::internal
#endif

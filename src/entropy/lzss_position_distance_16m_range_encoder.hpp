#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_16M_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_16M_RANGE_ENCODER_HPP
#include "entropy/contextual_dynamic_range_encoder.hpp"
namespace marc::entropy::internal {
struct LzssPositionDistance16mRangeEncodePlan {
  ContextualDynamicRangeEncodeResult details{};
  ContextualDynamicRangeDescriptor descriptor{};
  std::size_t aggregate_bytes{}, working_state_bytes{};
  ContextualDynamicRangeEncodeError error{
      ContextualDynamicRangeEncodeError::none};
};
struct LzssPositionDistance16mRangeEncodeResult {
  ContextualDynamicRangeEncodeResult details{};
  std::size_t bytes_committed{};
};
// Concrete working objects plus conservative simultaneously live control
// reservation. Logical retained storage only, not physical stack/RSS.
[[nodiscard]] std::size_t
lzss_position_distance_16m_range_encode_working_bytes() noexcept;
// Counts without writing. Full operation extent and BOTH complete payload
// capacities, working state and separately retained owner bytes are charged.
// Query success does not commit output/descriptor or validate raw history.
[[nodiscard]] LzssPositionDistance16mRangeEncodePlan
query_lzss_position_distance_16m_range_encode(
    std::span<const context::internal::ModeledOperation>,
    const core::DecoderLimits &, std::size_t output_capacity,
    std::size_t scratch_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
// Caller output AND descriptor unchanged on every failure; committed bytes=0.
// Scratch is private/discardable; inputs/configuration stable during the call.
// All full spans/metadata are disjoint. No allocation/public/frame admission.
[[nodiscard]] LzssPositionDistance16mRangeEncodeResult
encode_lzss_position_distance_16m_range_operations(
    std::span<const context::internal::ModeledOperation>,
    const core::DecoderLimits &, std::span<std::byte> output,
    std::span<std::byte> private_scratch,
    ContextualDynamicRangeDescriptor &descriptor,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::entropy::internal
#endif

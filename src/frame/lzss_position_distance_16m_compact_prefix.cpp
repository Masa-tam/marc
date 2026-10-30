#include "frame/lzss_position_distance_16m_compact_prefix.hpp"
#include "frame/position_range_compact_prefix_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using FrameRequirements = LzssPositionDistance16mFrameRequirements;
  using PreflightError = LzssPositionDistance16mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance16mRangeState;
  using SerializeError = LzssPositionDistance16mSerializeError;
  using SerializePlan = LzssPositionDistance16mSerializePlan;
  using SerializeResult = LzssPositionDistance16mSerializeResult;
  static constexpr auto context_count = 48;
  static constexpr auto raw_decisions = 9;
  static constexpr auto token_decisions = 34;
  static constexpr auto payload_factor = 18;
  static constexpr auto table_entries = 2610;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_16m_stream_semantics;
};
using Mechanics = PositionRangeCompactPrefixMechanics<Profile>;
using Error = Profile::PreflightError;
} // namespace
Error preflight_lzss_position_distance_16m_compact_prefix(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c,
    TypedContextFrameLayout &output,
    LzssPositionDistance16mFrameRequirements &requirements,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  return Mechanics::preflight(input, c, output, requirements, compact_capacity,
                              retained);
}
std::size_t lzss_position_distance_16m_compact_prefix_working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance16mSerializePlan
query_lzss_position_distance_16m_compact_prefix_serialize(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::size_t capacity,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  return Mechanics::query(layout, c, capacity, compact_capacity, retained);
}
LzssPositionDistance16mSerializeResult
serialize_lzss_position_distance_16m_compact_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::span<std::byte> output,
    std::size_t &written, std::size_t compact_capacity,
    std::size_t retained) noexcept {
  return Mechanics::serialize(layout, c, output, written, compact_capacity,
                              retained);
}
} // namespace marc::frame::internal

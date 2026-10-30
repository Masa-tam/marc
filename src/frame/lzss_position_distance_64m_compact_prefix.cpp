#include "frame/lzss_position_distance_64m_compact_prefix.hpp"
#include "frame/position_range_compact_prefix_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using FrameRequirements = LzssPositionDistance64mFrameRequirements;
  using PreflightError = LzssPositionDistance64mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance64mRangeState;
  using SerializeError = LzssPositionDistance64mSerializeError;
  using SerializePlan = LzssPositionDistance64mSerializePlan;
  using SerializeResult = LzssPositionDistance64mSerializeResult;
  static constexpr auto context_count = 50;
  static constexpr auto raw_decisions = 10;
  static constexpr auto token_decisions = 36;
  static constexpr auto payload_factor = 20;
  static constexpr auto table_entries = 2632;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_64m_stream_semantics;
};
using Mechanics = PositionRangeCompactPrefixMechanics<Profile>;
using Error = Profile::PreflightError;
} // namespace
Error preflight_lzss_position_distance_64m_compact_encode_prefix(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c,
    TypedContextFrameLayout &output,
    LzssPositionDistance64mFrameRequirements &requirements,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  return Mechanics::preflight(input, c, output, requirements, compact_capacity,
                              retained);
}
std::size_t lzss_position_distance_64m_compact_prefix_working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance64mSerializePlan
query_lzss_position_distance_64m_compact_prefix_serialize(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::size_t capacity,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  return Mechanics::query(layout, c, capacity, compact_capacity, retained);
}
LzssPositionDistance64mSerializeResult
serialize_lzss_position_distance_64m_compact_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::span<std::byte> output,
    std::size_t &written, std::size_t compact_capacity,
    std::size_t retained) noexcept {
  return Mechanics::serialize(layout, c, output, written, compact_capacity,
                              retained);
}
} // namespace marc::frame::internal

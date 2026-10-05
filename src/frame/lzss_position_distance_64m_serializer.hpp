#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_64M_SERIALIZER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_64M_SERIALIZER_HPP
#include "frame/lzss_position_distance_64m_preflight.hpp"
namespace marc::frame::internal {
enum class LzssPositionDistance64mSerializeError : std::uint8_t {
  none,
  output_too_small,
  overlapping_buffers,
  limit_exceeded,
  arithmetic_overflow,
  validation_error
};
struct LzssPositionDistance64mSerializePlan {
  std::size_t bytes_required{}, aggregate_bytes{}, working_state_bytes{};
  LzssPositionDistance64mPreflightError validation_error{
      LzssPositionDistance64mPreflightError::none};
  LzssPositionDistance64mSerializeError error{
      LzssPositionDistance64mSerializeError::none};
};
struct LzssPositionDistance64mSerializeResult {
  std::size_t bytes_committed{};
  LzssPositionDistance64mPreflightError validation_error{
      LzssPositionDistance64mPreflightError::none};
  LzssPositionDistance64mSerializeError error{
      LzssPositionDistance64mSerializeError::none};
};
[[nodiscard]] std::size_t
lzss_position_distance_64m_stream_serialize_working_bytes() noexcept;
[[nodiscard]] std::size_t
lzss_position_distance_64m_prefix_serialize_working_bytes() noexcept;
// Count/validate without caller writes; full output capacity + fixed internal
// scratch/control + separately retained owners. Existing decoder admission
// policy is also checked, independently of this serializer's storage ledger.
[[nodiscard]] LzssPositionDistance64mSerializePlan
query_lzss_position_distance_64m_stream_serialize(
    const TypedContextStreamHeader &, const core::DecoderLimits &,
    std::size_t output_capacity, std::size_t retained_owner_bytes = 0) noexcept;
[[nodiscard]] LzssPositionDistance64mSerializePlan
query_lzss_position_distance_64m_prefix_serialize(
    const TypedContextFrameLayout &, const TypedContextFrameValidationContext &,
    std::size_t output_capacity, std::size_t retained_owner_bytes = 0) noexcept;
// ANY failure preserves whole caller output AND bytes_written (committed0).
// Full regions/config/metadata disjoint and borrowed inputs stable. Internal
// fixed scratch only; no allocation or public/complete-frame admission.
[[nodiscard]] LzssPositionDistance64mSerializeResult
serialize_lzss_position_distance_64m_stream_header(
    const TypedContextStreamHeader &, const core::DecoderLimits &,
    std::span<std::byte>, std::size_t &bytes_written,
    std::size_t retained_owner_bytes = 0) noexcept;
[[nodiscard]] LzssPositionDistance64mSerializeResult
serialize_lzss_position_distance_64m_frame_prefix(
    const TypedContextFrameLayout &, const TypedContextFrameValidationContext &,
    std::span<std::byte>, std::size_t &bytes_written,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_FRAME_ENCODER_HPP
#include "context/lzss_position_distance_8m_mapper.hpp"
#include "dictionary/lzss_position_distance_8m_indexed.hpp"
#include "entropy/lzss_position_distance_8m_range_encoder.hpp"
#include "frame/lzss_position_distance_8m_serializer.hpp"
namespace marc::frame::internal {
struct LzssPositionDistance8mFrameEncodeWorkspace {
  std::span<dictionary::internal::LzssTypedToken> tokens, token_scratch;
  std::span<std::uint32_t> index;
  std::span<context::internal::ModeledOperation> operations, operation_scratch;
  std::span<std::byte> frame, payload_scratch;
};
enum class LzssPositionDistance8mFrameEncodeError : std::uint8_t {
  none,
  overlapping_buffers,
  arithmetic_overflow,
  limit_exceeded,
  invalid_position,
  invalid_stream,
  storage_too_small,
  parser_error,
  mapper_error,
  range_error,
  prefix_error,
  inconsistent_counts
};
struct LzssPositionDistance8mFrameEncodeResult {
  std::size_t bytes_committed{}, aggregate_bytes{}, working_state_bytes{};
  LzssPositionDistance8mFrameEncodeError error{
      LzssPositionDistance8mFrameEncodeError::none};
};
[[nodiscard]] std::size_t
lzss_position_distance_8m_frame_encode_working_bytes() noexcept;
// One nonempty exact raw frame, frame-local history. Full capacities of all
// distinct owners + concrete control/helper reservation + external retained.
// All buffers/config/metadata disjoint and borrowed inputs stable. No heap.
// Workspace discardable; ANY failure preserves WHOLE caller output, layout,
// bytes_written. Payload finish/count/prefix validation precedes one final
// complete-frame copy. Private only; no stream coordination/public admission.
[[nodiscard]] LzssPositionDistance8mFrameEncodeResult
encode_lzss_position_distance_8m_frame(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    const LzssPositionDistance8mFrameEncodeWorkspace &,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &bytes_written, std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

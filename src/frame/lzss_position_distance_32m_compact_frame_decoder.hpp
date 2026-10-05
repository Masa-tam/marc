#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_32M_COMPACT_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_32M_COMPACT_FRAME_DECODER_HPP
#include "context/lzss_position_distance_32m_compact_tokens.hpp"
#include "frame/lzss_position_distance_32m_preflight.hpp"
namespace marc::frame::internal {
enum class LzssPositionDistance32mCompactFrameDecodeError : std::uint8_t {
  none,
  preflight_error,
  truncated_frame,
  trailing_frame,
  storage_too_small,
  overlapping_buffers,
  limit_exceeded,
  arithmetic_overflow,
  token_error,
  raw_error
};
struct LzssPositionDistance32mCompactFrameDecodeResult {
  std::size_t bytes_consumed{}, raw_produced{};
  context::internal::LzssContextualRangeDecodeResult token_result{};
  LzssPositionDistance32mPreflightError preflight_error{
      LzssPositionDistance32mPreflightError::none};
  LzssPositionDistance32mCompactFrameDecodeError error{
      LzssPositionDistance32mCompactFrameDecodeError::none};
};
struct LzssPositionDistance32mCompactFrameDecodePlan {
  TypedContextFrameLayout layout{};
  LzssPositionDistance32mFrameRequirements prefix_requirements{};
  context::internal::LzssFieldContextValidationContext token_context{};
  context::internal::LzssPositionDistance32mCompactTokenRequirements
      token_requirements{};
  std::size_t retained_token_bytes{}, frame_state_bytes{};
  LzssPositionDistance32mPreflightError preflight_error{
      LzssPositionDistance32mPreflightError::none};
  LzssPositionDistance32mCompactFrameDecodeError error{
      LzssPositionDistance32mCompactFrameDecodeError::none};
};
// Concrete frame/result and private reconstruction state used by complete
// queries.
[[nodiscard]] std::size_t
lzss_position_distance_32m_compact_frame_working_bytes() noexcept;
// Requires one EXACT complete frame, including payload (no trailing bytes).
// Full compact-record/raw capacities and serialized extent are charged.
// Retained bytes include separately owned state and serialized-owner capacity
// beyond this view. Query success does not establish payload validity or permit
// publication.
[[nodiscard]] LzssPositionDistance32mCompactFrameDecodePlan
query_lzss_position_distance_32m_compact_frame_decode(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    std::size_t token_capacity, std::size_t token_scratch_capacity,
    std::size_t raw_output_capacity, std::size_t raw_scratch_capacity,
    std::size_t retained_state_bytes = 0) noexcept;
// Private finite frame helper, no stream/public admission. All full spans and
// layout output must be disjoint from each other/input/configuration. Inputs
// remain stable during a call. Raw output AND layout remain unchanged on error.
// Token buffers and raw scratch are private/discardable, including on failure.
[[nodiscard]] LzssPositionDistance32mCompactFrameDecodeResult
decode_lzss_position_distance_32m_compact_frame(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    std::span<std::byte> private_tokens,
    std::span<std::byte> private_token_scratch, std::span<std::byte> raw_output,
    std::span<std::byte> private_raw_scratch,
    TypedContextFrameLayout &layout_output,
    std::size_t retained_state_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

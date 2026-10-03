#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_TOKEN_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_TOKEN_FRAME_ENCODER_HPP
#include "dictionary/lzss_position_distance_8m_indexed.hpp"
#include "entropy/lzss_position_distance_8m_token_range_encoder.hpp"
#include "frame/lzss_position_distance_8m_serializer.hpp"
namespace marc::frame::internal {
// Every workspace span is private/discardable, including query token output.
struct LzssPositionDistance8mTokenFrameWorkspace {
  std::span<dictionary::internal::LzssTypedToken> tokens, token_scratch;
  std::span<std::uint32_t> index;
  std::span<std::byte> frame, payload_scratch;
};
enum class LzssPositionDistance8mTokenFrameError : std::uint8_t {
  none,
  overlapping_buffers,
  arithmetic_overflow,
  limit_exceeded,
  invalid_position,
  invalid_stream,
  storage_too_small,
  parser_error,
  token_error,
  range_error,
  prefix_error,
  inconsistent_counts
};
struct LzssPositionDistance8mTokenFramePlan {
  std::size_t aggregate_bytes{}, working_state_bytes{}, bytes_required{},
      token_count{};
  context::internal::LzssFieldContextValidationContext counts{};
  entropy::internal::LzssPositionDistance8mTokenRangePlan range_plan{};
  TypedContextFrameLayout layout{};
  LzssPositionDistance8mTokenFrameError error{
      LzssPositionDistance8mTokenFrameError::none};
};
struct LzssPositionDistance8mTokenFrameResult {
  std::size_t bytes_committed{}, aggregate_bytes{}, working_state_bytes{};
  LzssPositionDistance8mTokenFrameError error{
      LzssPositionDistance8mTokenFrameError::none};
};
[[nodiscard]] std::size_t
lzss_position_distance_8m_token_frame_working_bytes() noexcept;
// Nonempty exact raw frame. Query writes private token/index workspace only.
// Charges FULL raw/output/workspace extents and retained owners before
// traversal. No operation owners, allocation or caller publication. Zero output
// capacity reports shortage with exact counts/bytes_required when private
// storage fits. A successful plan never establishes payload validity or
// authorizes publication.
[[nodiscard]] LzssPositionDistance8mTokenFramePlan
query_lzss_position_distance_8m_token_frame_encode(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    const LzssPositionDistance8mTokenFrameWorkspace &,
    std::size_t output_capacity, std::size_t retained_owner_bytes = 0) noexcept;
// Full spans/configuration/metadata disjoint; borrowed input stable throughout.
// ANY failure leaves WHOLE caller output, layout and bytes_written unchanged,
// committed0. Scratch is discardable. Payload finish/complete-count agreement,
// prefix serialization and preflight precede one final complete-frame commit.
// Separate private implementation; existing frame/stream/public paths
// unchanged.
[[nodiscard]] LzssPositionDistance8mTokenFrameResult
encode_lzss_position_distance_8m_token_frame(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    const LzssPositionDistance8mTokenFrameWorkspace &,
    std::span<std::byte> output, TypedContextFrameLayout &,
    std::size_t &bytes_written, std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

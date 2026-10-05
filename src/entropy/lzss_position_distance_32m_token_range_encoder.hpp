#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_32M_TOKEN_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_32M_TOKEN_RANGE_ENCODER_HPP
#include "context/lzss_position_distance_32m_tokens.hpp"
#include "entropy/contextual_dynamic_range_format.hpp"
namespace marc::entropy::internal {
enum class LzssPositionDistance32mTokenRangeError : std::uint8_t {
  none,
  overlapping_buffers,
  arithmetic_overflow,
  limit_exceeded,
  invalid_parameters,
  invalid_token,
  token_count_mismatch,
  raw_size_mismatch,
  event_count_mismatch,
  decision_count_mismatch,
  payload_output_too_small,
  invalid_field,
  internal_error
};
struct LzssPositionDistance32mTokenRangeDetails {
  std::size_t token_count{}, token_index{}, operation_count{},
      operation_index{}, payload_size{};
  std::uint64_t raw_size{};
  std::uint32_t decision_count{};
  dictionary::internal::LzssTypedTokenError token_error{
      dictionary::internal::LzssTypedTokenError::none};
  context::internal::LzssFieldContextError field_error{
      context::internal::LzssFieldContextError::none};
  LzssPositionDistance32mTokenRangeError error{
      LzssPositionDistance32mTokenRangeError::none};
};
struct LzssPositionDistance32mTokenRangePlan {
  LzssPositionDistance32mTokenRangeDetails details{};
  ContextualDynamicRangeDescriptor descriptor{};
  std::size_t aggregate_bytes{}, working_state_bytes{};
};
struct LzssPositionDistance32mTokenRangeResult {
  LzssPositionDistance32mTokenRangeDetails details{};
  std::size_t bytes_committed{};
};
// Actual working objects and conservative simultaneous controls, not stack/RSS.
[[nodiscard]] std::size_t
lzss_position_distance_32m_token_range_working_bytes() noexcept;
// Counts/validates complete immutable tokens without materializing operations.
// Full token extent, BOTH payload capacities, working state and retained owners
// are admitted before traversal. Generic valid lengths3/4 remain accepted.
[[nodiscard]] LzssPositionDistance32mTokenRangePlan
query_lzss_position_distance_32m_token_range_encode(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssParameters &,
    const context::internal::LzssFieldContextValidationContext &,
    const core::DecoderLimits &, std::size_t output_capacity,
    std::size_t scratch_capacity,
    std::size_t retained_owner_bytes = 0) noexcept;
// ANY failure preserves every caller output byte AND descriptor; committed0.
// Private scratch is discardable. Full regions/configuration/descriptor must
// be disjoint. Borrowed inputs remain stable across count and write traversals.
// No allocation/public profile/frame integration or minimum-five restriction.
[[nodiscard]] LzssPositionDistance32mTokenRangeResult
encode_lzss_position_distance_32m_token_range(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssParameters &,
    const context::internal::LzssFieldContextValidationContext &,
    const core::DecoderLimits &, std::span<std::byte> output,
    std::span<std::byte> private_scratch, ContextualDynamicRangeDescriptor &,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::entropy::internal
#endif

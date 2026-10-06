#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_64M_REFERENCE_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_64M_REFERENCE_HPP
#include "dictionary/lzss_typed_token.hpp"
namespace marc::dictionary::internal {
enum class LzssPositionDistance64mParseError : std::uint8_t {
  none,
  invalid_parameters,
  limit_exceeded,
  arithmetic_overflow,
  output_too_small,
  overlapping_buffers
};
struct LzssPositionDistance64mParseMetadata {
  std::size_t token_count{}, raw_size{};
};
struct LzssPositionDistance64mParsePlan {
  LzssPositionDistance64mParseMetadata details{};
  std::size_t aggregate_bytes{}, working_state_bytes{};
  LzssPositionDistance64mParseError error{
      LzssPositionDistance64mParseError::none};
};
struct LzssPositionDistance64mParseResult {
  LzssPositionDistance64mParseMetadata details{};
  std::size_t tokens_committed{};
  LzssPositionDistance64mParseError error{
      LzssPositionDistance64mParseError::none};
};
[[nodiscard]] std::size_t
lzss_position_distance_64m_parse_working_bytes() noexcept;
// Nonempty finite raw frame; exhaustive longest match, nearest distance tie.
// Eligibility is five (canonical baseline cost9<2L), independent of valid
// wire lengths3/4. Full raw and BOTH token capacities + working + retained.
// Count-only query can take quadratic time; this is a correctness reference.
[[nodiscard]] LzssPositionDistance64mParsePlan
query_lzss_position_distance_64m_reference(
    std::span<const std::byte>, const LzssParameters &,
    const core::DecoderLimits &, std::size_t output_capacity,
    std::size_t scratch_capacity, std::size_t retained_owner_bytes = 0,
    std::uint64_t raw_already_committed = 0) noexcept;
// All full regions/config/metadata disjoint, stable borrowed input.
// ANY failure preserves caller output AND metadata bytes, committed0.
// Private scratch discardable. No allocation, no frame/public admission.
[[nodiscard]] LzssPositionDistance64mParseResult
tokenize_lzss_position_distance_64m_reference(
    std::span<const std::byte>, const LzssParameters &,
    const core::DecoderLimits &, std::span<LzssTypedToken> output,
    std::span<LzssTypedToken> private_scratch,
    LzssPositionDistance64mParseMetadata &,
    std::size_t retained_owner_bytes = 0,
    std::uint64_t raw_already_committed = 0) noexcept;
} // namespace marc::dictionary::internal
#endif

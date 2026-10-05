#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_COMPACT_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_COMPACT_TOKENS_HPP
#include "context/lzss_position_distance_32m_tokens.hpp"
namespace marc::context::internal {
struct LzssPositionDistance32mCompactTokenRequirements {
  std::size_t aggregate_bytes{}, working_state_bytes{};
  dictionary::internal::LzssTypedTokenError token_error{
      dictionary::internal::LzssTypedTokenError::none};
  LzssContextualRangeDecodeError error{LzssContextualRangeDecodeError::none};
};
// Concrete finite helper charge, including its simultaneously live query.
[[nodiscard]] std::size_t
lzss_position_distance_32m_compact_token_working_bytes() noexcept;
// Private records: literal 00 B; match 01 LE32(distance) LE32(length).
// Each supplied capacity must cover min(3R,9T); full capacities are charged;
// query success does not validate payload bytes.
[[nodiscard]] LzssPositionDistance32mCompactTokenRequirements
query_lzss_position_distance_32m_compact_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &,
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::size_t output_capacity, std::size_t scratch_capacity,
    std::size_t retained_state_bytes = 0) noexcept;
// Caller output and bytes_committed unchanged on ANY failure. Scratch is
// private/discardable and may retain validated tokens on failure; never
// reconstruct/publish that prefix. Full spans must be disjoint from each other
// and all borrowed input/configuration.
[[nodiscard]] LzssContextualRangeDecodeResult
decode_lzss_position_distance_32m_compact_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &,
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::span<std::byte> output, std::span<std::byte> private_scratch,
    std::size_t &bytes_committed,
    std::size_t retained_state_bytes = 0) noexcept;
} // namespace marc::context::internal
#endif

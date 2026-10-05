#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_TOKENS_HPP
#include "context/lzss_contextual_range_decoder.hpp"
namespace marc::context::internal {
struct LzssPositionDistance64mTokenCheck {
  std::uint64_t next_raw_size{};
  dictionary::internal::LzssTypedTokenError error{
      dictionary::internal::LzssTypedTokenError::none};
};
[[nodiscard]] dictionary::internal::LzssTypedTokenError
validate_lzss_position_distance_64m_parameters(
    const dictionary::internal::LzssParameters &,
    const core::DecoderLimits &) noexcept;
[[nodiscard]] LzssPositionDistance64mTokenCheck
validate_lzss_position_distance_64m_token(
    const dictionary::internal::LzssTypedToken &,
    const dictionary::internal::LzssParameters &,
    const dictionary::internal::LzssTypedTokenValidationContext &,
    const core::DecoderLimits &) noexcept;
struct LzssPositionDistance64mTokenRequirements {
  std::size_t aggregate_bytes{}, working_state_bytes{};
  dictionary::internal::LzssTypedTokenError token_error{
      dictionary::internal::LzssTypedTokenError::none};
  LzssContextualRangeDecodeError error{LzssContextualRangeDecodeError::none};
};
// Concrete finite helper charge, including its simultaneously live query.
[[nodiscard]] std::size_t
lzss_position_distance_64m_token_working_bytes() noexcept;
// Full capacities are charged; query success does not validate payload bytes.
[[nodiscard]] LzssPositionDistance64mTokenRequirements
query_lzss_position_distance_64m_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &,
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::size_t output_capacity, std::size_t scratch_capacity,
    std::size_t retained_state_bytes = 0) noexcept;
// Caller output unchanged on ANY failure. Scratch is private/discardable and
// may retain validated tokens on failure; never reconstruct/publish that
// prefix. Full spans must be disjoint from each other and all borrowed
// input/configuration.
[[nodiscard]] LzssContextualRangeDecodeResult
decode_lzss_position_distance_64m_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &,
    std::span<const std::byte>, const dictionary::internal::LzssParameters &,
    const LzssFieldContextValidationContext &, const core::DecoderLimits &,
    std::span<dictionary::internal::LzssTypedToken> output,
    std::span<dictionary::internal::LzssTypedToken> private_scratch,
    std::size_t retained_state_bytes = 0) noexcept;
} // namespace marc::context::internal
#endif

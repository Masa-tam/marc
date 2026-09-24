#ifndef MARC_DICTIONARY_LZSS_SHORT_MATCH_CANDIDATE_HPP
#define MARC_DICTIONARY_LZSS_SHORT_MATCH_CANDIDATE_HPP

#include "dictionary/lzss_typed_token.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssShortMatchCandidateError : std::uint8_t {
    none,
    invalid_eligibility,
    invalid_parameters,
    input_limit_exceeded,
    token_storage_limit_exceeded,
    output_too_small,
    overlapping_buffers,
    arithmetic_overflow,
    internal_error,
};

struct LzssShortMatchCandidateResult {
    std::size_t input_size{};
    std::size_t token_count{};
    std::size_t token_storage_size{};
    LzssTypedTokenError token_error{LzssTypedTokenError::none};
    LzssShortMatchCandidateError error{LzssShortMatchCandidateError::none};
};

// Private reference parser for the exact 64-KiB short-match identity.
// Eligibility 3, 4, or 5 affects only which already found match is emitted;
// the decoder never sees it. The source must remain stable across passes.
[[nodiscard]] LzssShortMatchCandidateResult plan_lzss_short_match_candidate(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint32_t minimum_eligible_length) noexcept;

[[nodiscard]] LzssShortMatchCandidateResult
tokenize_lzss_short_match_candidate(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint32_t minimum_eligible_length,
    std::span<LzssTypedToken> output) noexcept;

} // namespace marc::dictionary::internal

#endif

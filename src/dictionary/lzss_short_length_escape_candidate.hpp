#ifndef MARC_DICTIONARY_LZSS_SHORT_LENGTH_ESCAPE_CANDIDATE_HPP
#define MARC_DICTIONARY_LZSS_SHORT_LENGTH_ESCAPE_CANDIDATE_HPP

#include "dictionary/lzss_short_match_candidate.hpp"

namespace marc::dictionary::internal {

// Private exhaustive reference parser for the 64-KiB length-escape identity.
// Eligibility is an encoder choice, not a decoder-visible stream parameter.
[[nodiscard]] LzssShortMatchCandidateResult
plan_lzss_short_length_escape_candidate(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint32_t minimum_eligible_length) noexcept;

[[nodiscard]] LzssShortMatchCandidateResult
tokenize_lzss_short_length_escape_candidate(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::uint32_t minimum_eligible_length,
    std::span<LzssTypedToken> output) noexcept;

} // namespace marc::dictionary::internal

#endif

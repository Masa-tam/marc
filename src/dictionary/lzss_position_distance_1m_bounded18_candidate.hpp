#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_1M_BOUNDED18_CANDIDATE_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_1M_BOUNDED18_CANDIDATE_HPP
#include "dictionary/lzss_short_match_candidate.hpp"
#include "dictionary/lzss_position_distance_1m_bounded18_finder.hpp"
namespace marc::dictionary::internal {
// Private candidate, not selected by public streams. No allocation; charge full
// supplied capacities. Token output is unchanged on failure; finder is scratch.
[[nodiscard]] LzssShortMatchCandidateResult tokenize_lzss_position_distance_1m_bounded18_candidate(
    std::span<const std::byte> input,const LzssParameters& parameters,
    const core::DecoderLimits& limits,std::uint32_t eligibility,
    std::span<LzssTypedToken> tokens,std::span<std::byte> workspace) noexcept;
}
#endif

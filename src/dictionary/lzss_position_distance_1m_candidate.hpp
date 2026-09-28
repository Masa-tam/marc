#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_1M_CANDIDATE_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_1M_CANDIDATE_HPP
#include "dictionary/lzss_short_match_candidate.hpp"
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"

namespace marc::dictionary::internal {
enum class LzssPositionDistance1mSearch { exhaustive, indexed_reference, indexed };

// Private variant-9 candidate parser. Fixed eligibility 3/4/5, longest match,
// nearest equal-length tie. All supplied capacities are charged. Input/config
// stay stable and disjoint from writable storage. Finder workspace is scratch.
// At full N-token capacity parse once; smaller output uses a counting pass and
// preserves tokens on capacity/validation failure. No allocation is performed.
[[nodiscard]] LzssShortMatchCandidateResult tokenize_lzss_position_distance_1m_candidate(
    std::span<const std::byte> input,const LzssParameters& parameters,
    const core::DecoderLimits& limits,std::uint32_t eligibility,
    LzssPositionDistance1mSearch search,std::span<LzssTypedToken> tokens,
    std::span<std::byte> finder_workspace) noexcept;
}
#endif

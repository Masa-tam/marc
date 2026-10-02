#ifndef MARC_BENCHMARKS_LZSS_POSITION_DISTANCE_4M_TOKENIZER_SCOPE_HPP
#define MARC_BENCHMARKS_LZSS_POSITION_DISTANCE_4M_TOKENIZER_SCOPE_HPP
#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include <chrono>

namespace marc::dictionary::internal {
// Private, fixed-size diagnostic. Complete finder calls, not pure insertion or
// individual search branches. Counts include a count-only pass when required.
struct TokenizerScopeSample {
    double initialize_seconds{},find_seconds{},advance_seconds{};
    std::uint64_t raw_bytes{},token_count{},parse_passes{},find_calls{},advance_calls{},advanced_positions{};
    bool timed{};
};
inline constexpr std::size_t tokenizer_scope_transient_state_bytes=
    sizeof(TokenizerScopeSample)+sizeof(std::chrono::steady_clock::time_point);

// Reuses the admitted finder unchanged. No allocation. Full supplied capacities
// and staged diagnostic state are charged. On error tokens and report remain
// unchanged; workspace is scratch. Report must be disjoint from all arguments.
// Timed mode uses two clock reads per finder call and perturbs the measured path.
[[nodiscard]] LzssShortMatchCandidateResult tokenize_lzss_position_distance_4m_tokenizer_scope(
    std::span<const std::byte> input,const LzssParameters& parameters,
    const core::DecoderLimits& limits,std::uint32_t eligibility,
    std::span<LzssTypedToken> tokens,std::span<std::byte> workspace,
    TokenizerScopeSample& report,bool timed=false) noexcept;
}
#endif

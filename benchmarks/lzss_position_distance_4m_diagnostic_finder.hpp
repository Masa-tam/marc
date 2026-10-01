#ifndef MARC_BENCHMARKS_POSITION_DISTANCE_4M_DIAGNOSTIC_FINDER_HPP
#define MARC_BENCHMARKS_POSITION_DISTANCE_4M_DIAGNOSTIC_FINDER_HPP

#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_diagnostic_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_diagnostic_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant) noexcept;

// Instance-local diagnostic observations; these counts are not timings.
// A chain visit includes the terminating out-of-window candidate. Prefix
// comparisons count actual byte equality evaluations, including mismatches.
struct LzssPositionDistance4mFinderCounters {
    std::uint64_t initialized_words{}, find_calls{}, invalid_find_calls{};
    std::array<std::uint64_t,3> chain_visits{}, prefix_comparisons{}, insertions{};
    std::uint64_t fast_path_comparisons{}, candidate_filter_comparisons{};
    std::uint64_t extension_comparisons{}, extension_equal_bytes{};
    std::uint64_t advance_calls{}, invalid_advance_calls{}, advanced_positions{};
    bool overflow{};
};

// Private diagnostic copy of the admitted scalar finder. No public selection.
// Caller-owned arrays; query includes the enlarged instance and its counters.
// Query charges input + active arrays + finder state. Initialize failure preserves
// finder and workspace; only the queried prefix is borrowed on success.
class LzssPositionDistance4mDiagnosticFinder {
public:
    [[nodiscard]] LzssPositionDistance4mFinderCounters counters() const noexcept { return counters_; }
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssShortPrefixError initialize_lzss_position_distance_4m_diagnostic_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssPositionDistance4mDiagnosticFinder&, LzssTypedTokenVariant) noexcept;

    void count(std::uint64_t& value, std::uint64_t amount=1) const noexcept;
    mutable LzssPositionDistance4mFinderCounters counters_{};
    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::array<std::span<std::uint32_t>,3> heads_{}, links_{};
    std::size_t next_position_{};
};

static_assert(LzssMatchFinder<LzssPositionDistance4mDiagnosticFinder>);

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_diagnostic_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mDiagnosticFinder& finder) noexcept;

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_diagnostic_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mDiagnosticFinder& finder,
    LzssTypedTokenVariant variant) noexcept;

} // namespace marc::dictionary::internal

#endif

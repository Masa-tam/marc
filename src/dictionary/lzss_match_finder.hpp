#ifndef MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_match_finder_depth_histogram_size = 65;

enum class LzssMatchFinderStrategy : std::uint8_t {
    hash_chain_exact = 0,
    binary_tree_exact = 1,
};

enum class LzssMatchFinderWorkspaceError : std::uint8_t {
    none,
    unsupported_strategy,
    invalid_configuration,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
};

struct LzssMatchFinderWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{1};
    LzssMatchFinderStrategy strategy{
        LzssMatchFinderStrategy::hash_chain_exact};
    LzssMatchFinderWorkspaceError error{
        LzssMatchFinderWorkspaceError::none};
};

[[nodiscard]] bool is_supported_lzss_match_finder_strategy(
    LzssMatchFinderStrategy strategy) noexcept;

[[nodiscard]] std::size_t lzss_match_finder_workspace_alignment(
    LzssMatchFinderStrategy strategy) noexcept;

[[nodiscard]] LzssMatchFinderWorkspaceRequirements
calculate_lzss_match_finder_workspace(
    LzssMatchFinderStrategy strategy,
    std::size_t input_size,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

struct LzssMatch {
    std::uint32_t distance{};
    std::uint32_t length{};

    [[nodiscard]] bool operator==(const LzssMatch&) const noexcept = default;
};

struct LzssHashTreeComponentStatistics {
    std::uint64_t key_comparison_count{};
    std::uint64_t key_byte_comparison_count{};
    std::uint64_t lcp_byte_comparison_count{};
    std::uint64_t prefix_range_comparison_count{};
    std::uint64_t prefix_range_byte_comparison_count{};
    std::uint64_t lcp_skipped_byte_count{};
    std::uint64_t rotation_count{};
    std::uint64_t maximum_height{};
    bool overflowed{};
};

struct LzssMatchFinderStatistics {
    // Optional diagnostics. Strategy-specific fields remain zero for other
    // strategies.
    // A depth histogram's bin 0 is zero work items, bin 1 is one, and bin
    // n >= 2 is [2^(n-1), 2^n - 1]. Counter overflow saturates and sets
    // overflowed.
    std::uint64_t query_count{};
    std::uint64_t candidate_count{};
    std::uint64_t byte_comparison_count{};
    std::uint64_t hash_chain_prefix_match_count{};
    std::uint64_t hash_chain_prefix_mismatch_count{};
    std::uint64_t hash_chain_extension_byte_comparison_count{};
    std::uint64_t hash_chain_maximum_candidates_per_query{};
    std::array<std::uint64_t,
               lzss_match_finder_depth_histogram_size>
        hash_chain_query_depth_histogram{};
    std::uint64_t binary_tree_key_comparison_count{};
    std::uint64_t binary_tree_key_byte_comparison_count{};
    std::uint64_t binary_tree_lcp_byte_comparison_count{};
    std::uint64_t binary_tree_prefix_range_comparison_count{};
    std::uint64_t binary_tree_rotation_count{};
    std::uint64_t binary_tree_insertion_count{};
    std::uint64_t binary_tree_retirement_count{};
    std::uint64_t binary_tree_maximum_height{};
    std::uint64_t binary_tree_maximum_nodes_per_query{};
    std::array<std::uint64_t,
               lzss_match_finder_depth_histogram_size>
        binary_tree_query_depth_histogram{};
    std::uint64_t hash_tree_chain_query_count{};
    std::uint64_t hash_tree_chain_candidate_count{};
    std::uint64_t hash_tree_trigger_query_count{};
    std::uint64_t hash_tree_tree_query_count{};
    std::uint64_t hash_tree_promotion_count{};
    std::uint64_t hash_tree_promotion_trigger_candidate_count{};
    std::uint64_t hash_tree_promotion_maximum_trigger_candidates{};
    std::uint64_t hash_tree_promotion_build_node_count{};
    std::uint64_t hash_tree_tree_query_node_count{};
    std::uint64_t hash_tree_maximum_nodes_per_query{};
    std::uint64_t hash_tree_insertion_count{};
    std::uint64_t hash_tree_retirement_count{};
    std::uint64_t hash_tree_maximum_promoted_buckets{};
    std::uint64_t hash_tree_maximum_promoted_nodes{};
    std::uint64_t hash_tree_promotion_build_key_comparison_count{};
    std::uint64_t hash_tree_promotion_build_key_byte_comparison_count{};
    std::uint64_t hash_tree_promotion_build_rotation_count{};
    std::uint64_t hash_tree_tree_query_key_comparison_count{};
    std::uint64_t hash_tree_tree_query_key_byte_comparison_count{};
    std::uint64_t hash_tree_tree_query_lcp_byte_comparison_count{};
    std::uint64_t hash_tree_tree_query_prefix_range_comparison_count{};
    std::uint64_t hash_tree_tree_query_prefix_range_byte_comparison_count{};
    std::uint64_t hash_tree_tree_query_lcp_skipped_byte_count{};
    std::uint64_t hash_tree_maintenance_key_comparison_count{};
    std::uint64_t hash_tree_maintenance_key_byte_comparison_count{};
    std::uint64_t hash_tree_rotation_count{};
    std::uint64_t hash_tree_maximum_height{};
    std::array<std::uint64_t,
               lzss_match_finder_depth_histogram_size>
        hash_tree_chain_query_depth_histogram{};
    std::array<std::uint64_t,
               lzss_match_finder_depth_histogram_size>
        hash_tree_tree_query_depth_histogram{};
    bool overflowed{};
};

template <typename Finder>
concept LzssMatchFinder = requires(
    Finder& finder, std::size_t position,
    std::size_t next_position) {
    { finder.find_match(position) } noexcept
        -> std::same_as<LzssMatch>;
    { finder.advance(position, next_position) } noexcept
        -> std::same_as<void>;
};

class LzssExhaustiveMatchFinder {
public:
    LzssExhaustiveMatchFinder(
        std::span<const std::byte> input,
        const LzssParameters& parameters,
        LzssMatchFinderStatistics* statistics = nullptr) noexcept;

    // position must not exceed the input extent. The finder returns no match
    // at the exact end. Earlier positions need not be announced to this
    // stateless reference strategy.
    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;

    // Stateful indexed strategies use this boundary to index every consumed
    // raw position in [position, next_position). Exhaustive retains no index.
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    LzssMatchFinderStatistics* statistics_{};
};

static_assert(LzssMatchFinder<LzssExhaustiveMatchFinder>);

[[nodiscard]] bool lzss_match_is_beneficial(LzssMatch match) noexcept;

} // namespace marc::dictionary::internal

#endif

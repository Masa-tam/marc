#include "dictionary/lzss_match_finder.hpp"

#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] LzssMatchFinderWorkspaceError map_hash_chain_error(
    const LzssHashChainError error) noexcept {
    switch (error) {
    case LzssHashChainError::none:
        return LzssMatchFinderWorkspaceError::none;
    case LzssHashChainError::invalid_limits:
    case LzssHashChainError::invalid_parameters:
    case LzssHashChainError::workspace_too_small:
    case LzssHashChainError::misaligned_workspace:
    case LzssHashChainError::overlapping_buffers:
        return LzssMatchFinderWorkspaceError::invalid_configuration;
    case LzssHashChainError::input_limit_exceeded:
        return LzssMatchFinderWorkspaceError::input_limit_exceeded;
    case LzssHashChainError::arithmetic_overflow:
        return LzssMatchFinderWorkspaceError::arithmetic_overflow;
    case LzssHashChainError::workspace_limit_exceeded:
        return LzssMatchFinderWorkspaceError::workspace_limit_exceeded;
    }
    return LzssMatchFinderWorkspaceError::invalid_configuration;
}

[[nodiscard]] LzssMatchFinderWorkspaceError map_binary_tree_error(
    const LzssBinaryTreeError error) noexcept {
    switch (error) {
    case LzssBinaryTreeError::none:
        return LzssMatchFinderWorkspaceError::none;
    case LzssBinaryTreeError::invalid_limits:
    case LzssBinaryTreeError::invalid_parameters:
    case LzssBinaryTreeError::workspace_too_small:
    case LzssBinaryTreeError::misaligned_workspace:
    case LzssBinaryTreeError::overlapping_buffers:
    case LzssBinaryTreeError::invalid_position:
    case LzssBinaryTreeError::invalid_state:
        return LzssMatchFinderWorkspaceError::invalid_configuration;
    case LzssBinaryTreeError::input_limit_exceeded:
        return LzssMatchFinderWorkspaceError::input_limit_exceeded;
    case LzssBinaryTreeError::arithmetic_overflow:
        return LzssMatchFinderWorkspaceError::arithmetic_overflow;
    case LzssBinaryTreeError::workspace_limit_exceeded:
        return LzssMatchFinderWorkspaceError::workspace_limit_exceeded;
    }
    return LzssMatchFinderWorkspaceError::invalid_configuration;
}

void increment_statistic(
    LzssMatchFinderStatistics& statistics,
    std::uint64_t& value) noexcept {
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics.overflowed = true;
        return;
    }
    ++value;
}

} // namespace

bool is_supported_lzss_match_finder_strategy(
    const LzssMatchFinderStrategy strategy) noexcept {
    return strategy == LzssMatchFinderStrategy::hash_chain_exact
        || strategy == LzssMatchFinderStrategy::binary_tree_exact;
}

std::size_t lzss_match_finder_workspace_alignment(
    const LzssMatchFinderStrategy strategy) noexcept {
    switch (strategy) {
    case LzssMatchFinderStrategy::hash_chain_exact:
        return LzssHashChainWorkspaceRequirements{}.workspace_alignment;
    case LzssMatchFinderStrategy::binary_tree_exact:
        return LzssBinaryTreeWorkspaceRequirements{}.workspace_alignment;
    }
    return 0;
}

LzssMatchFinderWorkspaceRequirements calculate_lzss_match_finder_workspace(
    const LzssMatchFinderStrategy strategy,
    const std::size_t input_size,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssMatchFinderWorkspaceRequirements result{};
    result.strategy = strategy;
    switch (strategy) {
    case LzssMatchFinderStrategy::hash_chain_exact: {
        const auto selected = calculate_lzss_hash_chain_workspace(
            input_size, parameters, limits);
        result.error = map_hash_chain_error(selected.error);
        if (result.error == LzssMatchFinderWorkspaceError::none) {
            result.workspace_size = selected.workspace_size;
            result.workspace_alignment = selected.workspace_alignment;
        }
        return result;
    }
    case LzssMatchFinderStrategy::binary_tree_exact: {
        const auto selected = calculate_lzss_binary_tree_workspace(
            input_size, parameters, limits);
        result.error = map_binary_tree_error(selected.error);
        if (result.error == LzssMatchFinderWorkspaceError::none) {
            result.workspace_size = selected.workspace_size;
            result.workspace_alignment = selected.workspace_alignment;
        }
        return result;
    }
    }
    result.error = LzssMatchFinderWorkspaceError::unsupported_strategy;
    return result;
}

LzssExhaustiveMatchFinder::LzssExhaustiveMatchFinder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    LzssMatchFinderStatistics* const statistics) noexcept
    : input_(input), parameters_(parameters), statistics_(statistics) {}

LzssMatch LzssExhaustiveMatchFinder::find_match(
    const std::size_t position) const noexcept {
    LzssMatch best{};
    if (position >= input_.size()) return best;
    if (statistics_ != nullptr) {
        increment_statistic(*statistics_, statistics_->query_count);
    }
    const auto maximum_distance = std::min<std::size_t>(
        position, static_cast<std::size_t>(parameters_.window_size));
    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position,
        static_cast<std::size_t>(parameters_.max_match_length));
    for (std::size_t distance = 1; distance <= maximum_distance; ++distance) {
        if (statistics_ != nullptr) {
            increment_statistic(*statistics_, statistics_->candidate_count);
        }
        std::size_t length{};
        while (length < maximum_length) {
            if (statistics_ != nullptr) {
                increment_statistic(
                    *statistics_, statistics_->byte_comparison_count);
            }
            if (input_[position + length]
                != input_[position - distance + length]) break;
            ++length;
        }
        if (length >= parameters_.min_match_length
            && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
        }
    }
    return best;
}

void LzssExhaustiveMatchFinder::advance(
    const std::size_t, const std::size_t) noexcept {}

bool lzss_match_is_beneficial(const LzssMatch match) noexcept {
    return static_cast<std::size_t>(match.length)
        > lzss_match_size / lzss_literal_size;
}

} // namespace marc::dictionary::internal

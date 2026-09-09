#ifndef MARC_DICTIONARY_LZSS_WAVL_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_WAVL_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_wavl_tree_prefix_size = 5;
inline constexpr std::uint32_t lzss_wavl_tree_null_node = UINT32_MAX;
inline constexpr std::uint8_t lzss_wavl_tree_inactive_rank = UINT8_MAX;
inline constexpr std::size_t lzss_wavl_tree_no_position =
    std::numeric_limits<std::size_t>::max();

enum class LzssWavlTreeError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
    invalid_position,
    invalid_state,
    rank_overflow,
};

enum class LzssWavlTreeValidationError : std::uint8_t {
    none,
    uninitialized,
    invalid_root,
    invalid_active_count,
    invalid_inactive_node,
    invalid_index,
    invalid_parent,
    cycle_or_disconnected,
    invalid_order,
    invalid_rank_difference,
    invalid_leaf_rank,
    invalid_subtree_maximum,
    invalid_slot_position,
    invalid_protocol_state,
};

struct LzssWavlTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t node_count{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t rank_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssWavlTreeError error{LzssWavlTreeError::none};
};

struct LzssWavlTreeNodeSnapshot {
    std::uint32_t left{lzss_wavl_tree_null_node};
    std::uint32_t right{lzss_wavl_tree_null_node};
    std::uint32_t parent{lzss_wavl_tree_null_node};
    std::uint8_t rank{lzss_wavl_tree_inactive_rank};
    std::size_t position{lzss_wavl_tree_no_position};
    std::size_t subtree_maximum_position{lzss_wavl_tree_no_position};

    bool operator==(const LzssWavlTreeNodeSnapshot&) const = default;
};

struct LzssWavlTreeNeighborQueryResult {
    std::size_t predecessor_position{lzss_wavl_tree_no_position};
    std::size_t successor_position{lzss_wavl_tree_no_position};
    std::uint32_t predecessor_lcp{};
    std::uint32_t successor_lcp{};
    std::uint32_t maximum_lcp{};
    LzssWavlTreeError error{LzssWavlTreeError::none};

    bool operator==(const LzssWavlTreeNeighborQueryResult&) const = default;
};

struct LzssWavlTreeCandidateQueryResult {
    std::size_t candidate_position{lzss_wavl_tree_no_position};
    std::uint32_t length{};
    LzssWavlTreeError error{LzssWavlTreeError::none};

    bool operator==(const LzssWavlTreeCandidateQueryResult&) const = default;
};

[[nodiscard]] LzssWavlTreeError calculate_lzss_wavl_promoted_rank(
    std::uint8_t rank, std::uint8_t& promoted_rank) noexcept;

[[nodiscard]] LzssWavlTreeWorkspaceRequirements
calculate_lzss_wavl_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

class LzssWavlTreeMatchFinder {
public:
    LzssWavlTreeMatchFinder() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool empty() const noexcept { return active_node_count_ == 0; }
    [[nodiscard]] std::size_t input_size() const noexcept {
        return input_.size();
    }
    [[nodiscard]] std::size_t node_capacity() const noexcept {
        return left_.size();
    }
    [[nodiscard]] std::size_t active_node_count() const noexcept {
        return active_node_count_;
    }
    [[nodiscard]] std::uint32_t root_index() const noexcept { return root_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }

    [[nodiscard]] LzssWavlTreeNeighborQueryResult find_neighbors(
        std::size_t position) const noexcept;
    [[nodiscard]] LzssWavlTreeCandidateQueryResult find_candidate(
        std::size_t position) const noexcept;
    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssWavlTreeError initialize_lzss_wavl_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssWavlTreeMatchFinder&, LzssMatchFinderStatistics*) noexcept;
    friend LzssWavlTreeError insert_lzss_wavl_tree_position(
        LzssWavlTreeMatchFinder&, std::size_t) noexcept;
    friend LzssWavlTreeError remove_lzss_wavl_tree_position(
        LzssWavlTreeMatchFinder&, std::size_t) noexcept;
    friend LzssWavlTreeValidationError validate_lzss_wavl_tree(
        const LzssWavlTreeMatchFinder&) noexcept;
    friend LzssWavlTreeNodeSnapshot inspect_lzss_wavl_tree_node(
        const LzssWavlTreeMatchFinder&, std::uint32_t) noexcept;

    [[nodiscard]] int node_rank(std::uint32_t node) const noexcept;
    [[nodiscard]] int compare_positions(
        std::size_t left, std::size_t right) const noexcept;
    [[nodiscard]] std::uint32_t common_prefix_length(
        std::size_t left, std::size_t right) const noexcept;
    [[nodiscard]] int compare_prefix(
        std::size_t position, std::size_t query_position,
        std::uint32_t length) const noexcept;
    [[nodiscard]] LzssWavlTreeNeighborQueryResult find_neighbors_impl(
        std::size_t position, std::uint64_t* nodes_visited) const noexcept;
    void update_metadata(std::uint32_t node) noexcept;
    void update_metadata_upward(std::uint32_t node) noexcept;
    void replace_parent_child(
        std::uint32_t parent, std::uint32_t previous_child,
        std::uint32_t replacement) noexcept;
    [[nodiscard]] std::uint32_t rotate_left(std::uint32_t node) noexcept;
    [[nodiscard]] std::uint32_t rotate_right(std::uint32_t node) noexcept;
    [[nodiscard]] LzssWavlTreeError preflight_insertion_repair(
        std::uint32_t parent, bool insert_left) const noexcept;
    [[nodiscard]] LzssWavlTreeError preflight_removal(
        std::uint32_t removed, std::uint64_t& nodes_visited) const noexcept;
    void repair_after_insertion(std::uint32_t node) noexcept;
    void repair_after_removal(
        std::uint32_t parent, std::uint32_t replacement,
        bool replacement_is_left) noexcept;
    void clear_node(std::uint32_t node) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<std::uint8_t> rank_{};
    std::span<std::size_t> position_{};
    std::span<std::size_t> subtree_maximum_position_{};
    std::uint32_t root_{lzss_wavl_tree_null_node};
    std::size_t active_node_count_{};
    std::size_t next_position_{};
    LzssMatchFinderStatistics* statistics_{};
    bool initialized_{};
    bool state_valid_{};
};

static_assert(LzssMatchFinder<LzssWavlTreeMatchFinder>);

[[nodiscard]] LzssWavlTreeError initialize_lzss_wavl_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssWavlTreeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

[[nodiscard]] LzssWavlTreeError insert_lzss_wavl_tree_position(
    LzssWavlTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssWavlTreeError remove_lzss_wavl_tree_position(
    LzssWavlTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssWavlTreeValidationError validate_lzss_wavl_tree(
    const LzssWavlTreeMatchFinder& finder) noexcept;

[[nodiscard]] LzssWavlTreeNodeSnapshot inspect_lzss_wavl_tree_node(
    const LzssWavlTreeMatchFinder& finder, std::uint32_t node) noexcept;

} // namespace marc::dictionary::internal

#endif

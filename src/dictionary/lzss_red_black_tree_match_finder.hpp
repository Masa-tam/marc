#ifndef MARC_DICTIONARY_LZSS_RED_BLACK_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_RED_BLACK_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_red_black_tree_prefix_size = 5;
inline constexpr std::uint32_t lzss_red_black_tree_null_node = UINT32_MAX;
inline constexpr std::size_t lzss_red_black_tree_no_position =
    std::numeric_limits<std::size_t>::max();

enum class LzssRedBlackTreeNodeColor : std::uint8_t {
    black,
    red,
    inactive,
};

enum class LzssRedBlackTreeError : std::uint8_t {
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
};

enum class LzssRedBlackTreeValidationError : std::uint8_t {
    none,
    uninitialized,
    invalid_root,
    invalid_active_count,
    invalid_inactive_node,
    invalid_color,
    invalid_index,
    invalid_parent,
    cycle_or_disconnected,
    invalid_order,
    red_parent_violation,
    invalid_black_height,
    invalid_subtree_maximum,
    invalid_slot_position,
    invalid_protocol_state,
};

struct LzssRedBlackTreeNodeSnapshot {
    std::uint32_t left{lzss_red_black_tree_null_node};
    std::uint32_t right{lzss_red_black_tree_null_node};
    std::uint32_t parent{lzss_red_black_tree_null_node};
    LzssRedBlackTreeNodeColor color{LzssRedBlackTreeNodeColor::inactive};
    std::size_t position{std::numeric_limits<std::size_t>::max()};
    std::size_t subtree_maximum_position{
        std::numeric_limits<std::size_t>::max()};

    bool operator==(const LzssRedBlackTreeNodeSnapshot&) const = default;
};

struct LzssRedBlackTreeNeighborQueryResult {
    std::size_t predecessor_position{lzss_red_black_tree_no_position};
    std::size_t successor_position{lzss_red_black_tree_no_position};
    std::uint32_t predecessor_lcp{};
    std::uint32_t successor_lcp{};
    std::uint32_t maximum_lcp{};
    LzssRedBlackTreeError error{LzssRedBlackTreeError::none};

    bool operator==(const LzssRedBlackTreeNeighborQueryResult&) const = default;
};

struct LzssRedBlackTreeCandidateQueryResult {
    std::size_t candidate_position{lzss_red_black_tree_no_position};
    std::uint32_t length{};
    LzssRedBlackTreeError error{LzssRedBlackTreeError::none};

    bool operator==(const LzssRedBlackTreeCandidateQueryResult&) const = default;
};

struct LzssRedBlackTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t node_count{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t color_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssRedBlackTreeError error{LzssRedBlackTreeError::none};
};

[[nodiscard]] LzssRedBlackTreeWorkspaceRequirements
calculate_lzss_red_black_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

class LzssRedBlackTreeMatchFinder {
public:
    LzssRedBlackTreeMatchFinder() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool empty() const noexcept { return active_node_count_ == 0; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
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
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }
    [[nodiscard]] LzssRedBlackTreeNeighborQueryResult find_neighbors(
        std::size_t position) const noexcept;
    [[nodiscard]] LzssRedBlackTreeCandidateQueryResult find_candidate(
        std::size_t position) const noexcept;
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssRedBlackTreeError initialize_lzss_red_black_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssRedBlackTreeMatchFinder&, LzssMatchFinderStatistics*) noexcept;
    friend LzssRedBlackTreeError insert_lzss_red_black_tree_position(
        LzssRedBlackTreeMatchFinder&, std::size_t) noexcept;
    friend LzssRedBlackTreeError remove_lzss_red_black_tree_position(
        LzssRedBlackTreeMatchFinder&, std::size_t) noexcept;
    friend LzssRedBlackTreeValidationError validate_lzss_red_black_tree(
        const LzssRedBlackTreeMatchFinder&) noexcept;
    friend LzssRedBlackTreeNodeSnapshot inspect_lzss_red_black_tree_node(
        const LzssRedBlackTreeMatchFinder&, std::uint32_t) noexcept;

    [[nodiscard]] LzssRedBlackTreeNodeColor node_color(
        std::uint32_t node) const noexcept;
    void set_balancing_color(
        std::uint32_t node, LzssRedBlackTreeNodeColor color) noexcept;
    [[nodiscard]] int compare_positions(
        std::size_t left, std::size_t right) const noexcept;
    [[nodiscard]] std::uint32_t common_prefix_length(
        std::size_t left, std::size_t right) const noexcept;
    [[nodiscard]] int compare_prefix(
        std::size_t position, std::size_t query_position,
        std::uint32_t length) const noexcept;
    [[nodiscard]] LzssRedBlackTreeNeighborQueryResult find_neighbors_impl(
        std::size_t position, std::uint64_t* nodes_visited) const noexcept;
    void update_metadata(std::uint32_t node) noexcept;
    void update_metadata_upward(std::uint32_t node) noexcept;
    void replace_parent_child(
        std::uint32_t parent, std::uint32_t previous_child,
        std::uint32_t replacement) noexcept;
    [[nodiscard]] std::uint32_t rotate_left(std::uint32_t node) noexcept;
    [[nodiscard]] std::uint32_t rotate_right(std::uint32_t node) noexcept;
    void repair_after_insertion(std::uint32_t node) noexcept;
    void repair_after_removal(
        std::uint32_t node, std::uint32_t parent) noexcept;
    [[nodiscard]] std::uint32_t minimum_node(
        std::uint32_t node) const noexcept;
    void clear_node(std::uint32_t node) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<LzssRedBlackTreeNodeColor> color_{};
    std::span<std::size_t> position_{};
    std::span<std::size_t> subtree_maximum_position_{};
    std::uint32_t root_{lzss_red_black_tree_null_node};
    std::size_t active_node_count_{};
    std::size_t next_position_{};
    LzssMatchFinderStatistics* statistics_{};
    bool initialized_{};
    bool state_valid_{};
};

[[nodiscard]] LzssRedBlackTreeError
initialize_lzss_red_black_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssRedBlackTreeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

[[nodiscard]] LzssRedBlackTreeError insert_lzss_red_black_tree_position(
    LzssRedBlackTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssRedBlackTreeError remove_lzss_red_black_tree_position(
    LzssRedBlackTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssRedBlackTreeValidationError validate_lzss_red_black_tree(
    const LzssRedBlackTreeMatchFinder& finder) noexcept;

[[nodiscard]] LzssRedBlackTreeNodeSnapshot inspect_lzss_red_black_tree_node(
    const LzssRedBlackTreeMatchFinder& finder, std::uint32_t node) noexcept;

static_assert(sizeof(LzssRedBlackTreeNodeColor) == sizeof(std::uint8_t));
static_assert(LzssMatchFinder<LzssRedBlackTreeMatchFinder>);

} // namespace marc::dictionary::internal

#endif

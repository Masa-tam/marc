#ifndef MARC_DICTIONARY_LZSS_BINARY_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_BINARY_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_binary_tree_prefix_size = 5;
inline constexpr std::uint32_t lzss_binary_tree_null_node = UINT32_MAX;

enum class LzssBinaryTreeError : std::uint8_t {
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

enum class LzssBinaryTreeValidationError : std::uint8_t {
    none,
    uninitialized,
    invalid_root,
    invalid_active_count,
    invalid_inactive_node,
    invalid_index,
    invalid_parent,
    cycle_or_disconnected,
    invalid_order,
    invalid_height,
    unbalanced,
    invalid_subtree_maximum,
    invalid_slot_position,
};

struct LzssBinaryTreeNodeSnapshot {
    std::uint32_t left{lzss_binary_tree_null_node};
    std::uint32_t right{lzss_binary_tree_null_node};
    std::uint32_t parent{lzss_binary_tree_null_node};
    std::uint8_t height{};
    std::size_t position{std::numeric_limits<std::size_t>::max()};
    std::size_t subtree_maximum_position{
        std::numeric_limits<std::size_t>::max()};

    bool operator==(const LzssBinaryTreeNodeSnapshot&) const = default;
};

struct LzssBinaryTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t node_count{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t height_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssBinaryTreeError error{LzssBinaryTreeError::none};
};

[[nodiscard]] LzssBinaryTreeWorkspaceRequirements
calculate_lzss_binary_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

class LzssBinaryTreeMatchFinder {
public:
    LzssBinaryTreeMatchFinder() noexcept = default;

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

private:
    friend LzssBinaryTreeError initialize_lzss_binary_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssBinaryTreeMatchFinder&) noexcept;
    friend LzssBinaryTreeError insert_lzss_binary_tree_position(
        LzssBinaryTreeMatchFinder&, std::size_t) noexcept;
    friend LzssBinaryTreeError remove_lzss_binary_tree_position(
        LzssBinaryTreeMatchFinder&, std::size_t) noexcept;
    friend LzssBinaryTreeValidationError validate_lzss_binary_tree(
        const LzssBinaryTreeMatchFinder&) noexcept;
    friend LzssBinaryTreeNodeSnapshot inspect_lzss_binary_tree_node(
        const LzssBinaryTreeMatchFinder&, std::uint32_t) noexcept;

    [[nodiscard]] std::uint8_t node_height(std::uint32_t node) const noexcept;
    [[nodiscard]] int compare_positions(
        std::size_t left, std::size_t right) const noexcept;
    void update_metadata(std::uint32_t node) noexcept;
    void replace_parent_child(
        std::uint32_t parent, std::uint32_t previous_child,
        std::uint32_t replacement) noexcept;
    [[nodiscard]] std::uint32_t rotate_left(std::uint32_t node) noexcept;
    [[nodiscard]] std::uint32_t rotate_right(std::uint32_t node) noexcept;
    [[nodiscard]] int balance_factor(std::uint32_t node) const noexcept;
    void rebalance_from(std::uint32_t node) noexcept;
    [[nodiscard]] std::uint32_t minimum_node(std::uint32_t node) const noexcept;
    void clear_node(std::uint32_t node) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<std::uint8_t> height_{};
    std::span<std::size_t> position_{};
    std::span<std::size_t> subtree_maximum_position_{};
    std::uint32_t root_{lzss_binary_tree_null_node};
    std::size_t active_node_count_{};
    bool initialized_{};
};

[[nodiscard]] LzssBinaryTreeError initialize_lzss_binary_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssBinaryTreeMatchFinder& finder) noexcept;

[[nodiscard]] LzssBinaryTreeError insert_lzss_binary_tree_position(
    LzssBinaryTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssBinaryTreeError remove_lzss_binary_tree_position(
    LzssBinaryTreeMatchFinder& finder, std::size_t position) noexcept;

[[nodiscard]] LzssBinaryTreeValidationError validate_lzss_binary_tree(
    const LzssBinaryTreeMatchFinder& finder) noexcept;

[[nodiscard]] LzssBinaryTreeNodeSnapshot inspect_lzss_binary_tree_node(
    const LzssBinaryTreeMatchFinder& finder, std::uint32_t node) noexcept;

} // namespace marc::dictionary::internal

#endif

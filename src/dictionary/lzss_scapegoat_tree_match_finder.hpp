#ifndef MARC_DICTIONARY_LZSS_SCAPEGOAT_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_SCAPEGOAT_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_scapegoat_tree_prefix_size = 5;
inline constexpr std::uint32_t lzss_scapegoat_tree_null_node = UINT32_MAX;
inline constexpr std::size_t lzss_scapegoat_tree_no_position =
    std::numeric_limits<std::size_t>::max();

enum class LzssScapegoatTreeError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    unrepresentable_node_count,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
};

struct LzssScapegoatTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t node_count{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t subtree_size_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    std::size_t rebuild_scratch_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssScapegoatTreeError error{LzssScapegoatTreeError::none};
};

[[nodiscard]] LzssScapegoatTreeWorkspaceRequirements
calculate_lzss_scapegoat_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

class LzssScapegoatTreeMatchFinder {
public:
    LzssScapegoatTreeMatchFinder() noexcept = default;

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
    [[nodiscard]] std::size_t maximum_active_node_count() const noexcept {
        return maximum_active_node_count_;
    }
    [[nodiscard]] std::uint32_t root_index() const noexcept { return root_; }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }

private:
    friend LzssScapegoatTreeError
    initialize_lzss_scapegoat_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssScapegoatTreeMatchFinder&) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<std::uint32_t> subtree_size_{};
    std::span<std::size_t> position_{};
    std::span<std::size_t> subtree_maximum_position_{};
    std::span<std::uint32_t> rebuild_scratch_{};
    std::uint32_t root_{lzss_scapegoat_tree_null_node};
    std::size_t active_node_count_{};
    std::size_t maximum_active_node_count_{};
    std::size_t next_position_{};
    bool initialized_{};
    bool state_valid_{};
};

[[nodiscard]] LzssScapegoatTreeError
initialize_lzss_scapegoat_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssScapegoatTreeMatchFinder& finder) noexcept;

} // namespace marc::dictionary::internal

#endif

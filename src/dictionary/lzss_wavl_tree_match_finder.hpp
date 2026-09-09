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

private:
    friend LzssWavlTreeError initialize_lzss_wavl_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssWavlTreeMatchFinder&) noexcept;
    friend LzssWavlTreeNodeSnapshot inspect_lzss_wavl_tree_node(
        const LzssWavlTreeMatchFinder&, std::uint32_t) noexcept;

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
    bool initialized_{};
    bool state_valid_{};
};

[[nodiscard]] LzssWavlTreeError initialize_lzss_wavl_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssWavlTreeMatchFinder& finder) noexcept;

[[nodiscard]] LzssWavlTreeNodeSnapshot inspect_lzss_wavl_tree_node(
    const LzssWavlTreeMatchFinder& finder, std::uint32_t node) noexcept;

} // namespace marc::dictionary::internal

#endif

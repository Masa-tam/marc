#ifndef MARC_DICTIONARY_LZSS_RED_BLACK_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_RED_BLACK_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_red_black_tree_prefix_size = 5;
inline constexpr std::uint32_t lzss_red_black_tree_null_node = UINT32_MAX;

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

private:
    friend LzssRedBlackTreeError initialize_lzss_red_black_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssRedBlackTreeMatchFinder&) noexcept;

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
    bool initialized_{};
    bool state_valid_{};
};

[[nodiscard]] LzssRedBlackTreeError
initialize_lzss_red_black_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssRedBlackTreeMatchFinder& finder) noexcept;

static_assert(sizeof(LzssRedBlackTreeNodeColor) == sizeof(std::uint8_t));

} // namespace marc::dictionary::internal

#endif

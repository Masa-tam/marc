#include "dictionary/lzss_scapegoat_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool append_array(
    const std::size_t count, const std::size_t element_size,
    const std::size_t alignment, std::size_t& cursor,
    std::size_t& offset) noexcept {
    const auto remainder = cursor % alignment;
    const auto padding = remainder == 0 ? 0 : alignment - remainder;
    std::size_t bytes{};
    return core::checked_add(cursor, padding, offset)
        && core::checked_multiply(count, element_size, bytes)
        && core::checked_add(offset, bytes, cursor);
}

template<typename T>
[[nodiscard]] std::span<T> array_at(
    const std::span<std::byte> workspace, const std::size_t offset,
    const std::size_t count) noexcept {
    return {reinterpret_cast<T*>(workspace.data() + offset), count};
}

template<typename T>
void construct_array(const std::span<T> values, const T initial) noexcept {
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::construct_at(values.data() + index, initial);
    }
}

} // namespace

int LzssScapegoatTreeMatchFinder::compare_positions(
    const std::size_t left, const std::size_t right) const noexcept {
    const auto left_size = std::min<std::size_t>(
        input_.size() - left, parameters_.max_match_length);
    const auto right_size = std::min<std::size_t>(
        input_.size() - right, parameters_.max_match_length);
    const auto common_size = std::min(left_size, right_size);
    for (std::size_t index = 0; index < common_size; ++index) {
        const auto left_byte = std::to_integer<std::uint8_t>(
            input_[left + index]);
        const auto right_byte = std::to_integer<std::uint8_t>(
            input_[right + index]);
        if (left_byte < right_byte) return -1;
        if (left_byte > right_byte) return 1;
    }
    if (left_size < right_size) return -1;
    if (left_size > right_size) return 1;
    if (left < right) return -1;
    if (left > right) return 1;
    return 0;
}

void LzssScapegoatTreeMatchFinder::update_metadata(
    const std::uint32_t node) noexcept {
    std::uint32_t size{1};
    auto maximum = position_[node];
    if (left_[node] != lzss_scapegoat_tree_null_node) {
        size += subtree_size_[left_[node]];
        maximum = std::max(
            maximum, subtree_maximum_position_[left_[node]]);
    }
    if (right_[node] != lzss_scapegoat_tree_null_node) {
        size += subtree_size_[right_[node]];
        maximum = std::max(
            maximum, subtree_maximum_position_[right_[node]]);
    }
    subtree_size_[node] = size;
    subtree_maximum_position_[node] = maximum;
}

void LzssScapegoatTreeMatchFinder::update_metadata_upward(
    std::uint32_t node) noexcept {
    while (node != lzss_scapegoat_tree_null_node) {
        update_metadata(node);
        node = parent_[node];
    }
}

LzssScapegoatTreeWorkspaceRequirements
calculate_lzss_scapegoat_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssScapegoatTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssScapegoatTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssScapegoatTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssScapegoatTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_scapegoat_tree_prefix_size) {
        result.node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
    }
    if (result.node_count
        > static_cast<std::size_t>(lzss_scapegoat_tree_null_node)) {
        result.error = LzssScapegoatTreeError::unrepresentable_node_count;
        return result;
    }

    std::size_t cursor{};
    if (!append_array(result.node_count, sizeof(std::uint32_t),
                      alignof(std::uint32_t), cursor, result.left_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.right_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.parent_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor,
                         result.subtree_size_offset)
        || !append_array(result.node_count, sizeof(std::size_t),
                         alignof(std::size_t), cursor, result.position_offset)
        || !append_array(
            result.node_count, sizeof(std::size_t), alignof(std::size_t),
            cursor, result.subtree_maximum_position_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor,
                         result.rebuild_scratch_offset)) {
        result.error = LzssScapegoatTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssScapegoatTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssScapegoatTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssScapegoatTreeError initialize_lzss_scapegoat_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssScapegoatTreeMatchFinder& finder) noexcept {
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssScapegoatTreeError::none) {
        return required.error;
    }
    if (workspace.size() < required.workspace_size) {
        return LzssScapegoatTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssScapegoatTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssScapegoatTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssScapegoatTreeError::arithmetic_overflow;
    }

    LzssScapegoatTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssScapegoatTreeError::none;
    }

    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.subtree_size_ = array_at<std::uint32_t>(
        active_workspace, required.subtree_size_offset, required.node_count);
    initialized.position_ = array_at<std::size_t>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ = array_at<std::size_t>(
        active_workspace, required.subtree_maximum_position_offset,
        required.node_count);
    initialized.rebuild_scratch_ = array_at<std::uint32_t>(
        active_workspace, required.rebuild_scratch_offset,
        required.node_count);

    construct_array(initialized.left_, lzss_scapegoat_tree_null_node);
    construct_array(initialized.right_, lzss_scapegoat_tree_null_node);
    construct_array(initialized.parent_, lzss_scapegoat_tree_null_node);
    construct_array(initialized.subtree_size_, std::uint32_t{});
    construct_array(initialized.position_, lzss_scapegoat_tree_no_position);
    construct_array(
        initialized.subtree_maximum_position_,
        lzss_scapegoat_tree_no_position);
    construct_array(
        initialized.rebuild_scratch_, lzss_scapegoat_tree_null_node);

    finder = initialized;
    return LzssScapegoatTreeError::none;
}

LzssScapegoatTreeError insert_lzss_scapegoat_tree_position(
    LzssScapegoatTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || !finder.state_valid_
        || finder.left_.empty()) {
        return LzssScapegoatTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position
            < lzss_scapegoat_tree_prefix_size) {
        return LzssScapegoatTreeError::invalid_position;
    }
    const auto slot = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.position_[slot] != lzss_scapegoat_tree_no_position
        || finder.active_node_count_ == finder.left_.size()) {
        return LzssScapegoatTreeError::invalid_state;
    }

    auto parent = lzss_scapegoat_tree_null_node;
    auto current = finder.root_;
    int comparison{};
    std::size_t steps{};
    while (current != lzss_scapegoat_tree_null_node) {
        if (current >= finder.left_.size()
            || finder.position_[current]
                == lzss_scapegoat_tree_no_position
            || finder.position_[current] >= finder.input_.size()
            || finder.input_.size() - finder.position_[current]
                < lzss_scapegoat_tree_prefix_size
            || finder.position_[current] % finder.left_.size() != current
            || steps++ >= finder.active_node_count_) {
            return LzssScapegoatTreeError::invalid_state;
        }
        parent = current;
        comparison = finder.compare_positions(
            position, finder.position_[current]);
        current = comparison < 0 ? finder.left_[current]
                                 : finder.right_[current];
    }

    finder.left_[slot] = lzss_scapegoat_tree_null_node;
    finder.right_[slot] = lzss_scapegoat_tree_null_node;
    finder.parent_[slot] = parent;
    finder.subtree_size_[slot] = 1;
    finder.position_[slot] = position;
    finder.subtree_maximum_position_[slot] = position;
    if (parent == lzss_scapegoat_tree_null_node) {
        finder.root_ = slot;
    } else if (comparison < 0) {
        finder.left_[parent] = slot;
    } else {
        finder.right_[parent] = slot;
    }
    ++finder.active_node_count_;
    finder.maximum_active_node_count_ = std::max(
        finder.maximum_active_node_count_, finder.active_node_count_);
    finder.update_metadata_upward(parent);
    return LzssScapegoatTreeError::none;
}

LzssScapegoatTreeNodeSnapshot inspect_lzss_scapegoat_tree_node(
    const LzssScapegoatTreeMatchFinder& finder,
    const std::uint32_t node) noexcept {
    if (!finder.initialized_ || node >= finder.left_.size()) return {};
    return {finder.left_[node], finder.right_[node], finder.parent_[node],
            finder.subtree_size_[node], finder.position_[node],
            finder.subtree_maximum_position_[node]};
}

} // namespace marc::dictionary::internal

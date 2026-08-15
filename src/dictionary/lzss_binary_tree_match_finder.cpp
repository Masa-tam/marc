#include "dictionary/lzss_binary_tree_match_finder.hpp"

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

std::uint8_t LzssBinaryTreeMatchFinder::node_height(
    const std::uint32_t node) const noexcept {
    return node == lzss_binary_tree_null_node ? std::uint8_t{0}
                                               : height_[node];
}

int LzssBinaryTreeMatchFinder::compare_positions(
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

void LzssBinaryTreeMatchFinder::update_metadata(
    const std::uint32_t node) noexcept {
    const auto left = left_[node];
    const auto right = right_[node];
    const auto maximum_child_height = std::max(
        node_height(left), node_height(right));
    height_[node] = static_cast<std::uint8_t>(
        static_cast<unsigned>(maximum_child_height) + 1U);
    auto maximum_position = position_[node];
    if (left != lzss_binary_tree_null_node) {
        maximum_position = std::max(
            maximum_position, subtree_maximum_position_[left]);
    }
    if (right != lzss_binary_tree_null_node) {
        maximum_position = std::max(
            maximum_position, subtree_maximum_position_[right]);
    }
    subtree_maximum_position_[node] = maximum_position;
}

void LzssBinaryTreeMatchFinder::replace_parent_child(
    const std::uint32_t parent, const std::uint32_t previous_child,
    const std::uint32_t replacement) noexcept {
    if (parent == lzss_binary_tree_null_node) {
        root_ = replacement;
    } else if (left_[parent] == previous_child) {
        left_[parent] = replacement;
    } else {
        right_[parent] = replacement;
    }
    if (replacement != lzss_binary_tree_null_node) {
        parent_[replacement] = parent;
    }
}

std::uint32_t LzssBinaryTreeMatchFinder::rotate_left(
    const std::uint32_t node) noexcept {
    const auto promoted = right_[node];
    const auto transferred = left_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    left_[promoted] = node;
    parent_[node] = promoted;
    right_[node] = transferred;
    if (transferred != lzss_binary_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

std::uint32_t LzssBinaryTreeMatchFinder::rotate_right(
    const std::uint32_t node) noexcept {
    const auto promoted = left_[node];
    const auto transferred = right_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    right_[promoted] = node;
    parent_[node] = promoted;
    left_[node] = transferred;
    if (transferred != lzss_binary_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

int LzssBinaryTreeMatchFinder::balance_factor(
    const std::uint32_t node) const noexcept {
    return static_cast<int>(node_height(left_[node]))
        - static_cast<int>(node_height(right_[node]));
}

void LzssBinaryTreeMatchFinder::rebalance_from(
    std::uint32_t node) noexcept {
    while (node != lzss_binary_tree_null_node) {
        update_metadata(node);
        auto subtree_root = node;
        const auto balance = balance_factor(node);
        if (balance == 2) {
            const auto left = left_[node];
            if (balance_factor(left) < 0) {
                static_cast<void>(rotate_left(left));
            }
            subtree_root = rotate_right(node);
        } else if (balance == -2) {
            const auto right = right_[node];
            if (balance_factor(right) > 0) {
                static_cast<void>(rotate_right(right));
            }
            subtree_root = rotate_left(node);
        }
        node = parent_[subtree_root];
    }
}

std::uint32_t LzssBinaryTreeMatchFinder::minimum_node(
    std::uint32_t node) const noexcept {
    while (left_[node] != lzss_binary_tree_null_node) {
        node = left_[node];
    }
    return node;
}

void LzssBinaryTreeMatchFinder::clear_node(
    const std::uint32_t node) noexcept {
    left_[node] = lzss_binary_tree_null_node;
    right_[node] = lzss_binary_tree_null_node;
    parent_[node] = lzss_binary_tree_null_node;
    height_[node] = 0;
    position_[node] = std::numeric_limits<std::size_t>::max();
    subtree_maximum_position_[node] =
        std::numeric_limits<std::size_t>::max();
}

LzssBinaryTreeWorkspaceRequirements calculate_lzss_binary_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssBinaryTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssBinaryTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssBinaryTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssBinaryTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_binary_tree_prefix_size) {
        result.node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
    }

    std::size_t cursor{};
    if (!append_array(result.node_count, sizeof(std::uint32_t),
                      alignof(std::uint32_t), cursor, result.left_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.right_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.parent_offset)
        || !append_array(result.node_count, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.height_offset)
        || !append_array(result.node_count, sizeof(std::size_t),
                         alignof(std::size_t), cursor, result.position_offset)
        || !append_array(
            result.node_count, sizeof(std::size_t), alignof(std::size_t),
            cursor, result.subtree_maximum_position_offset)) {
        result.error = LzssBinaryTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssBinaryTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssBinaryTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssBinaryTreeError initialize_lzss_binary_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssBinaryTreeMatchFinder& finder) noexcept {
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssBinaryTreeError::none) return required.error;
    if (workspace.size() < required.workspace_size) {
        return LzssBinaryTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssBinaryTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssBinaryTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssBinaryTreeError::arithmetic_overflow;
    }

    LzssBinaryTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.initialized_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssBinaryTreeError::none;
    }

    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.height_ = array_at<std::uint8_t>(
        active_workspace, required.height_offset, required.node_count);
    initialized.position_ = array_at<std::size_t>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ = array_at<std::size_t>(
        active_workspace, required.subtree_maximum_position_offset,
        required.node_count);

    construct_array(initialized.left_, lzss_binary_tree_null_node);
    construct_array(initialized.right_, lzss_binary_tree_null_node);
    construct_array(initialized.parent_, lzss_binary_tree_null_node);
    construct_array(initialized.height_, std::uint8_t{0});
    construct_array(
        initialized.position_, std::numeric_limits<std::size_t>::max());
    construct_array(
        initialized.subtree_maximum_position_,
        std::numeric_limits<std::size_t>::max());

    finder = initialized;
    return LzssBinaryTreeError::none;
}

LzssBinaryTreeError insert_lzss_binary_tree_position(
    LzssBinaryTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || finder.left_.empty()) {
        return LzssBinaryTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position < lzss_binary_tree_prefix_size) {
        return LzssBinaryTreeError::invalid_position;
    }
    const auto slot = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.height_[slot] != 0
        || finder.active_node_count_ == finder.left_.size()) {
        return LzssBinaryTreeError::invalid_state;
    }

    auto parent = lzss_binary_tree_null_node;
    auto current = finder.root_;
    int comparison{};
    while (current != lzss_binary_tree_null_node) {
        parent = current;
        comparison = finder.compare_positions(
            position, finder.position_[current]);
        current = comparison < 0 ? finder.left_[current]
                                 : finder.right_[current];
    }

    finder.left_[slot] = lzss_binary_tree_null_node;
    finder.right_[slot] = lzss_binary_tree_null_node;
    finder.parent_[slot] = parent;
    finder.height_[slot] = 1;
    finder.position_[slot] = position;
    finder.subtree_maximum_position_[slot] = position;
    if (parent == lzss_binary_tree_null_node) {
        finder.root_ = slot;
    } else if (comparison < 0) {
        finder.left_[parent] = slot;
    } else {
        finder.right_[parent] = slot;
    }
    ++finder.active_node_count_;
    finder.rebalance_from(parent);
    return LzssBinaryTreeError::none;
}

LzssBinaryTreeError remove_lzss_binary_tree_position(
    LzssBinaryTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || finder.left_.empty()) {
        return LzssBinaryTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position < lzss_binary_tree_prefix_size) {
        return LzssBinaryTreeError::invalid_position;
    }
    const auto removed = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.height_[removed] == 0
        || finder.position_[removed] != position) {
        return LzssBinaryTreeError::invalid_state;
    }

    auto rebalance_start = lzss_binary_tree_null_node;
    if (finder.left_[removed] == lzss_binary_tree_null_node) {
        rebalance_start = finder.parent_[removed];
        finder.replace_parent_child(
            finder.parent_[removed], removed, finder.right_[removed]);
    } else if (finder.right_[removed] == lzss_binary_tree_null_node) {
        rebalance_start = finder.parent_[removed];
        finder.replace_parent_child(
            finder.parent_[removed], removed, finder.left_[removed]);
    } else {
        const auto successor = finder.minimum_node(finder.right_[removed]);
        if (finder.parent_[successor] != removed) {
            rebalance_start = finder.parent_[successor];
            finder.replace_parent_child(
                finder.parent_[successor], successor,
                finder.right_[successor]);
            finder.right_[successor] = finder.right_[removed];
            finder.parent_[finder.right_[successor]] = successor;
        } else {
            rebalance_start = successor;
        }
        finder.replace_parent_child(
            finder.parent_[removed], removed, successor);
        finder.left_[successor] = finder.left_[removed];
        finder.parent_[finder.left_[successor]] = successor;
        finder.update_metadata(successor);
    }

    finder.clear_node(removed);
    --finder.active_node_count_;
    finder.rebalance_from(rebalance_start);
    return LzssBinaryTreeError::none;
}

LzssBinaryTreeValidationError validate_lzss_binary_tree(
    const LzssBinaryTreeMatchFinder& finder) noexcept {
    if (!finder.initialized_) {
        return LzssBinaryTreeValidationError::uninitialized;
    }
    const auto capacity = finder.left_.size();
    if (finder.right_.size() != capacity
        || finder.parent_.size() != capacity
        || finder.height_.size() != capacity
        || finder.position_.size() != capacity
        || finder.subtree_maximum_position_.size() != capacity) {
        return LzssBinaryTreeValidationError::invalid_active_count;
    }
    if (finder.active_node_count_ > capacity) {
        return LzssBinaryTreeValidationError::invalid_active_count;
    }
    if ((finder.active_node_count_ == 0)
        != (finder.root_ == lzss_binary_tree_null_node)) {
        return LzssBinaryTreeValidationError::invalid_root;
    }
    if (finder.root_ != lzss_binary_tree_null_node) {
        if (finder.root_ >= capacity) {
            return LzssBinaryTreeValidationError::invalid_root;
        }
        if (finder.parent_[finder.root_] != lzss_binary_tree_null_node) {
            return LzssBinaryTreeValidationError::invalid_root;
        }
    }

    std::size_t observed_active{};
    for (std::size_t raw_node = 0; raw_node < capacity; ++raw_node) {
        const auto node = static_cast<std::uint32_t>(raw_node);
        if (finder.height_[node] == 0) {
            if (finder.left_[node] != lzss_binary_tree_null_node
                || finder.right_[node] != lzss_binary_tree_null_node
                || finder.parent_[node] != lzss_binary_tree_null_node
                || finder.position_[node]
                    != std::numeric_limits<std::size_t>::max()
                || finder.subtree_maximum_position_[node]
                    != std::numeric_limits<std::size_t>::max()) {
                return LzssBinaryTreeValidationError::invalid_inactive_node;
            }
            continue;
        }
        ++observed_active;
        if (capacity == 0 || finder.position_[node] >= finder.input_.size()
            || finder.input_.size() - finder.position_[node]
                < lzss_binary_tree_prefix_size
            || finder.position_[node] % capacity != node) {
            return LzssBinaryTreeValidationError::invalid_slot_position;
        }

        const auto left = finder.left_[node];
        const auto right = finder.right_[node];
        for (const auto child : {left, right}) {
            if (child != lzss_binary_tree_null_node
                && (child >= capacity || finder.height_[child] == 0)) {
                return LzssBinaryTreeValidationError::invalid_index;
            }
            if (child != lzss_binary_tree_null_node
                && (finder.position_[child] >= finder.input_.size()
                    || finder.input_.size() - finder.position_[child]
                        < lzss_binary_tree_prefix_size
                    || finder.position_[child] % capacity != child)) {
                return LzssBinaryTreeValidationError::invalid_slot_position;
            }
        }
        if ((left != lzss_binary_tree_null_node
             && finder.parent_[left] != node)
            || (right != lzss_binary_tree_null_node
                && finder.parent_[right] != node)) {
            return LzssBinaryTreeValidationError::invalid_parent;
        }
        if ((left != lzss_binary_tree_null_node
             && finder.compare_positions(
                    finder.position_[left], finder.position_[node]) >= 0)
            || (right != lzss_binary_tree_null_node
                && finder.compare_positions(
                       finder.position_[right], finder.position_[node]) <= 0)) {
            return LzssBinaryTreeValidationError::invalid_order;
        }

        const auto left_height = finder.node_height(left);
        const auto right_height = finder.node_height(right);
        const auto expected_height = static_cast<unsigned>(
            std::max(left_height, right_height)) + 1U;
        if (finder.height_[node] != expected_height) {
            return LzssBinaryTreeValidationError::invalid_height;
        }
        const auto balance = static_cast<int>(left_height)
            - static_cast<int>(right_height);
        if (balance < -1 || balance > 1) {
            return LzssBinaryTreeValidationError::unbalanced;
        }
        auto expected_maximum = finder.position_[node];
        if (left != lzss_binary_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[left]);
        }
        if (right != lzss_binary_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[right]);
        }
        if (finder.subtree_maximum_position_[node] != expected_maximum) {
            return LzssBinaryTreeValidationError::invalid_subtree_maximum;
        }

        auto ancestor = node;
        std::size_t steps{};
        while (ancestor != finder.root_ && steps <= finder.active_node_count_) {
            ancestor = finder.parent_[ancestor];
            if (ancestor == lzss_binary_tree_null_node
                || ancestor >= capacity || finder.height_[ancestor] == 0) {
                return LzssBinaryTreeValidationError::cycle_or_disconnected;
            }
            ++steps;
        }
        if (ancestor != finder.root_) {
            return LzssBinaryTreeValidationError::cycle_or_disconnected;
        }
    }
    if (observed_active != finder.active_node_count_) {
        return LzssBinaryTreeValidationError::invalid_active_count;
    }
    return LzssBinaryTreeValidationError::none;
}

LzssBinaryTreeNodeSnapshot inspect_lzss_binary_tree_node(
    const LzssBinaryTreeMatchFinder& finder,
    const std::uint32_t node) noexcept {
    if (!finder.initialized_ || node >= finder.left_.size()) return {};
    return {finder.left_[node], finder.right_[node], finder.parent_[node],
            finder.height_[node], finder.position_[node],
            finder.subtree_maximum_position_[node]};
}

} // namespace marc::dictionary::internal

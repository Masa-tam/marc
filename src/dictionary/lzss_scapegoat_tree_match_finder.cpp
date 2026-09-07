#include "dictionary/lzss_scapegoat_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

enum class RebuildAttachment : std::uint8_t {
    root,
    left,
    right,
};

struct RebuildTask {
    std::size_t begin{};
    std::size_t end{};
    std::uint32_t parent{lzss_scapegoat_tree_null_node};
    RebuildAttachment attachment{RebuildAttachment::root};
};

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

std::uint32_t LzssScapegoatTreeMatchFinder::common_prefix_length(
    const std::size_t left, const std::size_t right) const noexcept {
    const auto maximum = std::min({
        input_.size() - left,
        input_.size() - right,
        static_cast<std::size_t>(parameters_.max_match_length)});
    std::size_t length{};
    while (length < maximum
           && input_[left + length] == input_[right + length]) {
        ++length;
    }
    return static_cast<std::uint32_t>(length);
}

int LzssScapegoatTreeMatchFinder::compare_prefix(
    const std::size_t position, const std::size_t query_position,
    const std::uint32_t length) const noexcept {
    for (std::size_t index = 0; index < length; ++index) {
        const auto byte = std::to_integer<std::uint8_t>(
            input_[position + index]);
        const auto query_byte = std::to_integer<std::uint8_t>(
            input_[query_position + index]);
        if (byte < query_byte) return -1;
        if (byte > query_byte) return 1;
    }
    return 0;
}

bool LzssScapegoatTreeMatchFinder::valid_query_node(
    const std::uint32_t node,
    const std::size_t query_position) const noexcept {
    return node < left_.size()
        && position_[node] != lzss_scapegoat_tree_no_position
        && position_[node] < query_position;
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

bool LzssScapegoatTreeMatchFinder::update_metadata_upward(
    std::uint32_t node) noexcept {
    std::size_t steps{};
    while (node != lzss_scapegoat_tree_null_node) {
        if (node >= left_.size()
            || position_[node] == lzss_scapegoat_tree_no_position
            || steps++ >= active_node_count_
            || (left_[node] != lzss_scapegoat_tree_null_node
                && (left_[node] >= left_.size()
                    || position_[left_[node]]
                        == lzss_scapegoat_tree_no_position
                    || parent_[left_[node]] != node))
            || (right_[node] != lzss_scapegoat_tree_null_node
                && (right_[node] >= left_.size()
                    || position_[right_[node]]
                        == lzss_scapegoat_tree_no_position
                    || parent_[right_[node]] != node))) {
            return false;
        }
        update_metadata(node);
        node = parent_[node];
    }
    return true;
}

void LzssScapegoatTreeMatchFinder::replace_parent_child(
    const std::uint32_t parent, const std::uint32_t previous_child,
    const std::uint32_t replacement) noexcept {
    if (parent == lzss_scapegoat_tree_null_node) {
        root_ = replacement;
    } else if (left_[parent] == previous_child) {
        left_[parent] = replacement;
    } else {
        right_[parent] = replacement;
    }
    if (replacement != lzss_scapegoat_tree_null_node) {
        parent_[replacement] = parent;
    }
}

void LzssScapegoatTreeMatchFinder::clear_node(
    const std::uint32_t node) noexcept {
    left_[node] = lzss_scapegoat_tree_null_node;
    right_[node] = lzss_scapegoat_tree_null_node;
    parent_[node] = lzss_scapegoat_tree_null_node;
    subtree_size_[node] = 0;
    position_[node] = lzss_scapegoat_tree_no_position;
    subtree_maximum_position_[node] = lzss_scapegoat_tree_no_position;
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
    std::size_t depth{};
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
        ++depth;
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
    if (!finder.update_metadata_upward(parent)) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }

    const auto depth_limit = 2U * std::bit_width(
        finder.maximum_active_node_count_);
    if (depth <= depth_limit) return LzssScapegoatTreeError::none;

    auto path_child = slot;
    auto ancestor = finder.parent_[path_child];
    while (ancestor != lzss_scapegoat_tree_null_node) {
        const auto child_size = static_cast<std::uint64_t>(
            finder.subtree_size_[path_child]);
        const auto ancestor_size = static_cast<std::uint64_t>(
            finder.subtree_size_[ancestor]);
        if (UINT64_C(3) * child_size > UINT64_C(2) * ancestor_size) {
            const auto error = rebuild_lzss_scapegoat_tree_subtree(
                finder, ancestor);
            if (error != LzssScapegoatTreeError::none) {
                finder.state_valid_ = false;
            }
            return error;
        }
        path_child = ancestor;
        ancestor = finder.parent_[ancestor];
    }
    finder.state_valid_ = false;
    return LzssScapegoatTreeError::invalid_state;
}

LzssScapegoatTreeError rebuild_lzss_scapegoat_tree_subtree(
    LzssScapegoatTreeMatchFinder& finder,
    const std::uint32_t subtree_root) noexcept {
    if (!finder.initialized_ || !finder.state_valid_
        || subtree_root >= finder.left_.size()
        || finder.position_[subtree_root]
            == lzss_scapegoat_tree_no_position) {
        return LzssScapegoatTreeError::invalid_state;
    }
    const auto expected_count = static_cast<std::size_t>(
        finder.subtree_size_[subtree_root]);
    if (expected_count == 0 || expected_count > finder.active_node_count_
        || expected_count > finder.rebuild_scratch_.size()) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }

    const auto boundary_parent = finder.parent_[subtree_root];
    auto root_attachment = RebuildAttachment::root;
    if (boundary_parent != lzss_scapegoat_tree_null_node) {
        if (boundary_parent >= finder.left_.size()
            || finder.position_[boundary_parent]
                == lzss_scapegoat_tree_no_position) {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
        if (finder.left_[boundary_parent] == subtree_root) {
            root_attachment = RebuildAttachment::left;
        } else if (finder.right_[boundary_parent] == subtree_root) {
            root_attachment = RebuildAttachment::right;
        } else {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
    } else if (finder.root_ != subtree_root) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }

    auto previous = boundary_parent;
    auto current = subtree_root;
    std::size_t count{};
    std::uint64_t transitions{};
    const auto transition_limit = UINT64_C(2) * expected_count + 1U;
    while (current != boundary_parent) {
        if (current == lzss_scapegoat_tree_null_node
            || current >= finder.left_.size()
            || finder.position_[current]
                == lzss_scapegoat_tree_no_position
            || finder.position_[current] >= finder.input_.size()
            || finder.position_[current] % finder.left_.size() != current
            || transitions++ >= transition_limit) {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
        const auto left = finder.left_[current];
        const auto right = finder.right_[current];
        if ((left != lzss_scapegoat_tree_null_node
             && (left >= finder.left_.size()
                 || finder.parent_[left] != current))
            || (right != lzss_scapegoat_tree_null_node
                && (right >= finder.left_.size()
                    || finder.parent_[right] != current))) {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }

        auto next = lzss_scapegoat_tree_null_node;
        bool emit{};
        if (previous == finder.parent_[current]) {
            if (left != lzss_scapegoat_tree_null_node) {
                next = left;
            } else {
                emit = true;
                next = right != lzss_scapegoat_tree_null_node
                    ? right : finder.parent_[current];
            }
        } else if (previous == left) {
            emit = true;
            next = right != lzss_scapegoat_tree_null_node
                ? right : finder.parent_[current];
        } else if (previous == right) {
            next = finder.parent_[current];
        } else {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }

        if (emit) {
            if (count >= expected_count
                || (count != 0
                    && finder.compare_positions(
                        finder.position_[finder.rebuild_scratch_[count - 1U]],
                        finder.position_[current]) >= 0)) {
                finder.state_valid_ = false;
                return LzssScapegoatTreeError::invalid_state;
            }
            finder.rebuild_scratch_[count++] = current;
        }
        previous = current;
        current = next;
    }
    if (count != expected_count) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }

    std::array<RebuildTask, lzss_scapegoat_tree_rebuild_task_capacity>
        tasks{};
    std::size_t task_count{1};
    tasks[0] = {0, expected_count, boundary_parent, root_attachment};
    auto rebuilt_root = lzss_scapegoat_tree_null_node;
    while (task_count != 0) {
        const auto task = tasks[--task_count];
        const auto middle = task.begin
            + (task.end - task.begin - 1U) / 2U;
        const auto node = finder.rebuild_scratch_[middle];
        finder.left_[node] = lzss_scapegoat_tree_null_node;
        finder.right_[node] = lzss_scapegoat_tree_null_node;
        finder.parent_[node] = task.parent;
        finder.subtree_size_[node] = 1;
        finder.subtree_maximum_position_[node] = finder.position_[node];
        if (task.attachment == RebuildAttachment::root) {
            finder.root_ = node;
            rebuilt_root = node;
        } else if (task.attachment == RebuildAttachment::left) {
            finder.left_[task.parent] = node;
            if (task.parent == boundary_parent) rebuilt_root = node;
        } else {
            finder.right_[task.parent] = node;
            if (task.parent == boundary_parent) rebuilt_root = node;
        }

        if (middle + 1U < task.end) {
            if (task_count == tasks.size()) {
                finder.state_valid_ = false;
                return LzssScapegoatTreeError::invalid_state;
            }
            tasks[task_count++] = {
                middle + 1U, task.end, node, RebuildAttachment::right};
        }
        if (task.begin < middle) {
            if (task_count == tasks.size()) {
                finder.state_valid_ = false;
                return LzssScapegoatTreeError::invalid_state;
            }
            tasks[task_count++] = {
                task.begin, middle, node, RebuildAttachment::left};
        }
    }

    previous = boundary_parent;
    current = rebuilt_root;
    transitions = 0;
    while (current != boundary_parent) {
        if (current == lzss_scapegoat_tree_null_node
            || transitions++ >= transition_limit) {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
        auto next = lzss_scapegoat_tree_null_node;
        if (previous == finder.parent_[current]) {
            if (finder.left_[current] != lzss_scapegoat_tree_null_node) {
                next = finder.left_[current];
            } else if (finder.right_[current]
                       != lzss_scapegoat_tree_null_node) {
                next = finder.right_[current];
            } else {
                finder.update_metadata(current);
                next = finder.parent_[current];
            }
        } else if (previous == finder.left_[current]) {
            if (finder.right_[current] != lzss_scapegoat_tree_null_node) {
                next = finder.right_[current];
            } else {
                finder.update_metadata(current);
                next = finder.parent_[current];
            }
        } else if (previous == finder.right_[current]) {
            finder.update_metadata(current);
            next = finder.parent_[current];
        } else {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
        previous = current;
        current = next;
    }
    if (!finder.update_metadata_upward(boundary_parent)) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }
    return LzssScapegoatTreeError::none;
}

LzssScapegoatTreeError remove_lzss_scapegoat_tree_position(
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
    const auto removed = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.position_[removed] != position
        || finder.active_node_count_ == 0
        || finder.maximum_active_node_count_ < finder.active_node_count_) {
        return LzssScapegoatTreeError::invalid_state;
    }

    const auto removed_parent = finder.parent_[removed];
    const auto removed_left = finder.left_[removed];
    const auto removed_right = finder.right_[removed];
    if ((removed_parent == lzss_scapegoat_tree_null_node
         && finder.root_ != removed)
        || (removed_parent != lzss_scapegoat_tree_null_node
            && (removed_parent >= finder.left_.size()
                || finder.position_[removed_parent]
                    == lzss_scapegoat_tree_no_position
                || ((finder.left_[removed_parent] == removed)
                    == (finder.right_[removed_parent] == removed))))
        || (removed_left != lzss_scapegoat_tree_null_node
            && (removed_left >= finder.left_.size()
                || finder.position_[removed_left]
                    == lzss_scapegoat_tree_no_position
                || finder.parent_[removed_left] != removed))
        || (removed_right != lzss_scapegoat_tree_null_node
            && (removed_right >= finder.left_.size()
                || finder.position_[removed_right]
                    == lzss_scapegoat_tree_no_position
                || finder.parent_[removed_right] != removed))
        || (removed_left != lzss_scapegoat_tree_null_node
            && removed_left == removed_right)) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }

    auto successor = lzss_scapegoat_tree_null_node;
    auto successor_parent = lzss_scapegoat_tree_null_node;
    auto successor_right = lzss_scapegoat_tree_null_node;
    if (removed_left != lzss_scapegoat_tree_null_node
        && removed_right != lzss_scapegoat_tree_null_node) {
        successor = removed_right;
        std::size_t steps{};
        while (true) {
            if (successor >= finder.left_.size()
                || finder.position_[successor]
                    == lzss_scapegoat_tree_no_position
                || steps++ >= finder.active_node_count_) {
                finder.state_valid_ = false;
                return LzssScapegoatTreeError::invalid_state;
            }
            const auto left = finder.left_[successor];
            const auto right = finder.right_[successor];
            if ((left != lzss_scapegoat_tree_null_node
                 && (left >= finder.left_.size()
                     || finder.position_[left]
                         == lzss_scapegoat_tree_no_position
                     || finder.parent_[left] != successor))
                || (right != lzss_scapegoat_tree_null_node
                    && (right >= finder.left_.size()
                        || finder.position_[right]
                            == lzss_scapegoat_tree_no_position
                        || finder.parent_[right] != successor))) {
                finder.state_valid_ = false;
                return LzssScapegoatTreeError::invalid_state;
            }
            if (left == lzss_scapegoat_tree_null_node) break;
            successor = left;
        }
        successor_parent = finder.parent_[successor];
        successor_right = finder.right_[successor];
        if (successor_right != lzss_scapegoat_tree_null_node
            && (successor_right >= finder.left_.size()
                || finder.position_[successor_right]
                    == lzss_scapegoat_tree_no_position
                || finder.parent_[successor_right] != successor)) {
            finder.state_valid_ = false;
            return LzssScapegoatTreeError::invalid_state;
        }
    }

    auto metadata_start = removed_parent;
    if (removed_left == lzss_scapegoat_tree_null_node) {
        finder.replace_parent_child(
            removed_parent, removed, removed_right);
    } else if (removed_right == lzss_scapegoat_tree_null_node) {
        finder.replace_parent_child(
            removed_parent, removed, removed_left);
    } else {
        if (successor_parent != removed) {
            finder.replace_parent_child(
                successor_parent, successor, successor_right);
            finder.right_[successor] = removed_right;
            finder.parent_[removed_right] = successor;
            metadata_start = successor_parent;
        } else {
            metadata_start = successor;
        }
        finder.replace_parent_child(removed_parent, removed, successor);
        finder.left_[successor] = removed_left;
        finder.parent_[removed_left] = successor;
    }

    finder.clear_node(removed);
    --finder.active_node_count_;
    if (finder.active_node_count_ == 0) {
        finder.root_ = lzss_scapegoat_tree_null_node;
        finder.maximum_active_node_count_ = 0;
        return LzssScapegoatTreeError::none;
    }

    if (!finder.update_metadata_upward(metadata_start)) {
        finder.state_valid_ = false;
        return LzssScapegoatTreeError::invalid_state;
    }
    const auto active = static_cast<std::uint64_t>(
        finder.active_node_count_);
    const auto maximum = static_cast<std::uint64_t>(
        finder.maximum_active_node_count_);
    if (UINT64_C(3) * active >= UINT64_C(2) * maximum) {
        return LzssScapegoatTreeError::none;
    }
    const auto error = rebuild_lzss_scapegoat_tree_subtree(
        finder, finder.root_);
    if (error != LzssScapegoatTreeError::none) {
        finder.state_valid_ = false;
        return error;
    }
    finder.maximum_active_node_count_ = finder.active_node_count_;
    return LzssScapegoatTreeError::none;
}

LzssScapegoatTreeNeighborQueryResult
LzssScapegoatTreeMatchFinder::find_neighbors(
    const std::size_t position) const noexcept {
    LzssScapegoatTreeNeighborQueryResult result{};
    if (!initialized_ || !state_valid_) {
        result.error = LzssScapegoatTreeError::invalid_state;
        return result;
    }
    if (position > input_.size()) {
        result.error = LzssScapegoatTreeError::invalid_position;
        return result;
    }
    if (input_.size() - position < lzss_scapegoat_tree_prefix_size
        || root_ == lzss_scapegoat_tree_null_node) {
        return result;
    }

    auto predecessor = lzss_scapegoat_tree_null_node;
    auto successor = lzss_scapegoat_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_scapegoat_tree_null_node) {
        if (steps++ >= active_node_count_
            || !valid_query_node(current, position)) {
            result.error = LzssScapegoatTreeError::invalid_state;
            return result;
        }
        const auto comparison = compare_positions(position, position_[current]);
        if (comparison == 0) {
            result.error = LzssScapegoatTreeError::invalid_state;
            return result;
        }
        if (comparison < 0) {
            successor = current;
            current = left_[current];
        } else {
            predecessor = current;
            current = right_[current];
        }
    }

    if (predecessor != lzss_scapegoat_tree_null_node) {
        result.predecessor_position = position_[predecessor];
        result.predecessor_lcp = common_prefix_length(
            position, result.predecessor_position);
    }
    if (successor != lzss_scapegoat_tree_null_node) {
        result.successor_position = position_[successor];
        result.successor_lcp = common_prefix_length(
            position, result.successor_position);
    }
    result.maximum_lcp = std::max(
        result.predecessor_lcp, result.successor_lcp);
    return result;
}

LzssScapegoatTreeCandidateQueryResult
LzssScapegoatTreeMatchFinder::find_candidate(
    const std::size_t position) const noexcept {
    LzssScapegoatTreeCandidateQueryResult result{};
    const auto neighbors = find_neighbors(position);
    if (neighbors.error != LzssScapegoatTreeError::none) {
        result.error = neighbors.error;
        return result;
    }
    if (neighbors.maximum_lcp < parameters_.min_match_length) return result;

    auto split = lzss_scapegoat_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_scapegoat_tree_null_node) {
        if (steps++ >= active_node_count_
            || !valid_query_node(current, position)) {
            result.error = LzssScapegoatTreeError::invalid_state;
            return result;
        }
        const auto comparison = compare_prefix(
            position_[current], position, neighbors.maximum_lcp);
        if (comparison < 0) {
            current = right_[current];
        } else if (comparison > 0) {
            current = left_[current];
        } else {
            split = current;
            break;
        }
    }
    if (split == lzss_scapegoat_tree_null_node) {
        result.error = LzssScapegoatTreeError::invalid_state;
        return result;
    }

    auto maximum_position = position_[split];
    current = left_[split];
    steps = 0;
    while (current != lzss_scapegoat_tree_null_node) {
        if (steps++ >= active_node_count_
            || !valid_query_node(current, position)) {
            result.error = LzssScapegoatTreeError::invalid_state;
            return result;
        }
        const auto comparison = compare_prefix(
            position_[current], position, neighbors.maximum_lcp);
        if (comparison < 0) {
            current = right_[current];
        } else if (comparison > 0) {
            current = left_[current];
        } else {
            maximum_position = std::max(maximum_position, position_[current]);
            const auto inside = right_[current];
            if (inside != lzss_scapegoat_tree_null_node) {
                if (!valid_query_node(inside, position)
                    || subtree_maximum_position_[inside]
                        == lzss_scapegoat_tree_no_position
                    || subtree_maximum_position_[inside] >= position) {
                    result.error = LzssScapegoatTreeError::invalid_state;
                    return result;
                }
                maximum_position = std::max(
                    maximum_position, subtree_maximum_position_[inside]);
            }
            current = left_[current];
        }
    }

    current = right_[split];
    steps = 0;
    while (current != lzss_scapegoat_tree_null_node) {
        if (steps++ >= active_node_count_
            || !valid_query_node(current, position)) {
            result.error = LzssScapegoatTreeError::invalid_state;
            return result;
        }
        const auto comparison = compare_prefix(
            position_[current], position, neighbors.maximum_lcp);
        if (comparison < 0) {
            current = right_[current];
        } else if (comparison > 0) {
            current = left_[current];
        } else {
            maximum_position = std::max(maximum_position, position_[current]);
            const auto inside = left_[current];
            if (inside != lzss_scapegoat_tree_null_node) {
                if (!valid_query_node(inside, position)
                    || subtree_maximum_position_[inside]
                        == lzss_scapegoat_tree_no_position
                    || subtree_maximum_position_[inside] >= position) {
                    result.error = LzssScapegoatTreeError::invalid_state;
                    return result;
                }
                maximum_position = std::max(
                    maximum_position, subtree_maximum_position_[inside]);
            }
            current = right_[current];
        }
    }

    result.candidate_position = maximum_position;
    result.length = neighbors.maximum_lcp;
    return result;
}

LzssMatch LzssScapegoatTreeMatchFinder::find_match(
    const std::size_t position) const noexcept {
    const auto candidate = find_candidate(position);
    if (candidate.error != LzssScapegoatTreeError::none
        || candidate.candidate_position == lzss_scapegoat_tree_no_position
        || candidate.candidate_position >= position) {
        return {};
    }
    const auto distance = position - candidate.candidate_position;
    if (distance > std::numeric_limits<std::uint32_t>::max()) return {};
    return {static_cast<std::uint32_t>(distance), candidate.length};
}

LzssScapegoatTreeValidationError validate_lzss_scapegoat_tree(
    const LzssScapegoatTreeMatchFinder& finder) noexcept {
    if (!finder.initialized_) {
        return LzssScapegoatTreeValidationError::uninitialized;
    }
    if (!finder.state_valid_) {
        return LzssScapegoatTreeValidationError::invalid_protocol_state;
    }
    const auto capacity = finder.left_.size();
    if (finder.right_.size() != capacity
        || finder.parent_.size() != capacity
        || finder.subtree_size_.size() != capacity
        || finder.position_.size() != capacity
        || finder.subtree_maximum_position_.size() != capacity
        || finder.rebuild_scratch_.size() != capacity) {
        return LzssScapegoatTreeValidationError::invalid_layout;
    }
    if (finder.active_node_count_ > capacity) {
        return LzssScapegoatTreeValidationError::invalid_active_count;
    }
    if (finder.maximum_active_node_count_ < finder.active_node_count_
        || finder.maximum_active_node_count_ > capacity
        || (finder.active_node_count_ == 0
            && finder.maximum_active_node_count_ != 0)) {
        return LzssScapegoatTreeValidationError::invalid_q;
    }
    if ((finder.active_node_count_ == 0)
        != (finder.root_ == lzss_scapegoat_tree_null_node)) {
        return LzssScapegoatTreeValidationError::invalid_root;
    }
    if (finder.root_ != lzss_scapegoat_tree_null_node
        && (finder.root_ >= capacity
            || finder.position_[finder.root_]
                == lzss_scapegoat_tree_no_position
            || finder.parent_[finder.root_]
                != lzss_scapegoat_tree_null_node)) {
        return LzssScapegoatTreeValidationError::invalid_root;
    }

    std::size_t observed_active{};
    for (std::size_t raw_node = 0; raw_node < capacity; ++raw_node) {
        const auto node = static_cast<std::uint32_t>(raw_node);
        if (finder.position_[node] == lzss_scapegoat_tree_no_position) {
            if (finder.left_[node] != lzss_scapegoat_tree_null_node
                || finder.right_[node] != lzss_scapegoat_tree_null_node
                || finder.parent_[node] != lzss_scapegoat_tree_null_node
                || finder.subtree_size_[node] != 0
                || finder.subtree_maximum_position_[node]
                    != lzss_scapegoat_tree_no_position) {
                return LzssScapegoatTreeValidationError::invalid_inactive_node;
            }
            continue;
        }
        ++observed_active;
        if (capacity == 0 || finder.position_[node] >= finder.input_.size()
            || finder.input_.size() - finder.position_[node]
                < lzss_scapegoat_tree_prefix_size
            || finder.position_[node] % capacity != node) {
            return LzssScapegoatTreeValidationError::invalid_slot_position;
        }

        const auto left = finder.left_[node];
        const auto right = finder.right_[node];
        if (left != lzss_scapegoat_tree_null_node && left == right) {
            return LzssScapegoatTreeValidationError::invalid_index;
        }
        for (const auto child : {left, right}) {
            if (child != lzss_scapegoat_tree_null_node
                && (child >= capacity
                    || finder.position_[child]
                        == lzss_scapegoat_tree_no_position)) {
                return LzssScapegoatTreeValidationError::invalid_index;
            }
            if (child != lzss_scapegoat_tree_null_node
                && finder.parent_[child] != node) {
                return LzssScapegoatTreeValidationError::invalid_parent;
            }
        }
        const auto parent = finder.parent_[node];
        if (node != finder.root_) {
            if (parent == lzss_scapegoat_tree_null_node
                || parent >= capacity
                || finder.position_[parent]
                    == lzss_scapegoat_tree_no_position
                || ((finder.left_[parent] == node)
                    == (finder.right_[parent] == node))) {
                return LzssScapegoatTreeValidationError::invalid_parent;
            }
        }

        auto ancestor = node;
        std::size_t steps{};
        while (ancestor != finder.root_) {
            if (steps++ >= finder.active_node_count_) {
                return LzssScapegoatTreeValidationError::cycle_or_disconnected;
            }
            ancestor = finder.parent_[ancestor];
            if (ancestor == lzss_scapegoat_tree_null_node
                || ancestor >= capacity
                || finder.position_[ancestor]
                    == lzss_scapegoat_tree_no_position) {
                return LzssScapegoatTreeValidationError::cycle_or_disconnected;
            }
        }

        if ((left != lzss_scapegoat_tree_null_node
             && finder.compare_positions(
                    finder.position_[left], finder.position_[node]) >= 0)
            || (right != lzss_scapegoat_tree_null_node
                && finder.compare_positions(
                       finder.position_[right], finder.position_[node]) <= 0)) {
            return LzssScapegoatTreeValidationError::invalid_order;
        }

        std::uint64_t expected_size{1};
        auto expected_maximum = finder.position_[node];
        if (left != lzss_scapegoat_tree_null_node) {
            expected_size += finder.subtree_size_[left];
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[left]);
        }
        if (right != lzss_scapegoat_tree_null_node) {
            expected_size += finder.subtree_size_[right];
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[right]);
        }
        if (expected_size != finder.subtree_size_[node]) {
            return LzssScapegoatTreeValidationError::invalid_subtree_size;
        }
        if (expected_maximum
            != finder.subtree_maximum_position_[node]) {
            return LzssScapegoatTreeValidationError::invalid_subtree_maximum;
        }
    }
    if (observed_active != finder.active_node_count_) {
        return LzssScapegoatTreeValidationError::invalid_active_count;
    }
    if (finder.root_ == lzss_scapegoat_tree_null_node) {
        if (finder.next_position_ > finder.input_.size()) {
            return LzssScapegoatTreeValidationError::invalid_protocol_state;
        }
        if (finder.next_position_ == 0) {
            return LzssScapegoatTreeValidationError::none;
        }
        const auto first =
            finder.next_position_ > finder.parameters_.window_size
            ? finder.next_position_ - finder.parameters_.window_size : 0U;
        const auto indexable_count =
            finder.input_.size() < lzss_scapegoat_tree_prefix_size ? 0U
            : finder.input_.size() - lzss_scapegoat_tree_prefix_size + 1U;
        const auto end = std::min(finder.next_position_, indexable_count);
        return end <= first
            ? LzssScapegoatTreeValidationError::none
            : LzssScapegoatTreeValidationError::invalid_active_interval;
    }
    if (finder.subtree_size_[finder.root_]
        != finder.active_node_count_) {
        return LzssScapegoatTreeValidationError::invalid_active_count;
    }

    auto previous_position = lzss_scapegoat_tree_no_position;
    auto previous = lzss_scapegoat_tree_null_node;
    auto current = finder.root_;
    std::size_t ordered_count{};
    std::uint64_t transitions{};
    const auto transition_limit =
        UINT64_C(2) * finder.active_node_count_ + 1U;
    while (current != lzss_scapegoat_tree_null_node) {
        if (current >= capacity || transitions++ >= transition_limit) {
            return LzssScapegoatTreeValidationError::cycle_or_disconnected;
        }
        const auto left = finder.left_[current];
        const auto right = finder.right_[current];
        auto next = lzss_scapegoat_tree_null_node;
        bool emit{};
        if (previous == finder.parent_[current]) {
            if (left != lzss_scapegoat_tree_null_node) {
                next = left;
            } else {
                emit = true;
                next = right != lzss_scapegoat_tree_null_node
                    ? right : finder.parent_[current];
            }
        } else if (previous == left) {
            emit = true;
            next = right != lzss_scapegoat_tree_null_node
                ? right : finder.parent_[current];
        } else if (previous == right) {
            next = finder.parent_[current];
        } else {
            return LzssScapegoatTreeValidationError::cycle_or_disconnected;
        }
        if (emit) {
            if (ordered_count != 0
                && finder.compare_positions(
                    previous_position, finder.position_[current]) >= 0) {
                return LzssScapegoatTreeValidationError::invalid_order;
            }
            previous_position = finder.position_[current];
            ++ordered_count;
        }
        previous = current;
        current = next;
    }
    if (ordered_count != finder.active_node_count_) {
        return LzssScapegoatTreeValidationError::cycle_or_disconnected;
    }
    if (finder.next_position_ > finder.input_.size()) {
        return LzssScapegoatTreeValidationError::invalid_protocol_state;
    }
    if (finder.next_position_ != 0) {
        const auto first = finder.next_position_ > finder.parameters_.window_size
            ? finder.next_position_ - finder.parameters_.window_size : 0U;
        const auto indexable_count =
            finder.input_.size() < lzss_scapegoat_tree_prefix_size ? 0U
            : finder.input_.size() - lzss_scapegoat_tree_prefix_size + 1U;
        const auto end = std::min(finder.next_position_, indexable_count);
        const auto expected_active = end > first ? end - first : 0U;
        if (expected_active != finder.active_node_count_) {
            return LzssScapegoatTreeValidationError::invalid_active_interval;
        }
        for (std::size_t node = 0; node < capacity; ++node) {
            if (finder.position_[node] != lzss_scapegoat_tree_no_position
                && (finder.position_[node] < first
                    || finder.position_[node] >= end)) {
                return LzssScapegoatTreeValidationError::invalid_active_interval;
            }
        }
    }
    return LzssScapegoatTreeValidationError::none;
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

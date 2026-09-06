#include "dictionary/lzss_red_black_tree_match_finder.hpp"

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

LzssRedBlackTreeNodeColor LzssRedBlackTreeMatchFinder::node_color(
    const std::uint32_t node) const noexcept {
    return node == lzss_red_black_tree_null_node
        ? LzssRedBlackTreeNodeColor::black : color_[node];
}

int LzssRedBlackTreeMatchFinder::compare_positions(
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

std::uint32_t LzssRedBlackTreeMatchFinder::common_prefix_length(
    const std::size_t left, const std::size_t right) const noexcept {
    const auto maximum = std::min({
        input_.size() - left,
        input_.size() - right,
        static_cast<std::size_t>(parameters_.max_match_length)});
    std::size_t length{};
    while (length < maximum && input_[left + length] == input_[right + length]) {
        ++length;
    }
    return static_cast<std::uint32_t>(length);
}

int LzssRedBlackTreeMatchFinder::compare_prefix(
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

void LzssRedBlackTreeMatchFinder::update_metadata(
    const std::uint32_t node) noexcept {
    auto maximum = position_[node];
    if (left_[node] != lzss_red_black_tree_null_node) {
        maximum = std::max(
            maximum, subtree_maximum_position_[left_[node]]);
    }
    if (right_[node] != lzss_red_black_tree_null_node) {
        maximum = std::max(
            maximum, subtree_maximum_position_[right_[node]]);
    }
    subtree_maximum_position_[node] = maximum;
}

void LzssRedBlackTreeMatchFinder::update_metadata_upward(
    std::uint32_t node) noexcept {
    while (node != lzss_red_black_tree_null_node) {
        update_metadata(node);
        node = parent_[node];
    }
}

void LzssRedBlackTreeMatchFinder::replace_parent_child(
    const std::uint32_t parent, const std::uint32_t previous_child,
    const std::uint32_t replacement) noexcept {
    if (parent == lzss_red_black_tree_null_node) {
        root_ = replacement;
    } else if (left_[parent] == previous_child) {
        left_[parent] = replacement;
    } else {
        right_[parent] = replacement;
    }
    if (replacement != lzss_red_black_tree_null_node) {
        parent_[replacement] = parent;
    }
}

std::uint32_t LzssRedBlackTreeMatchFinder::rotate_left(
    const std::uint32_t node) noexcept {
    const auto promoted = right_[node];
    const auto transferred = left_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    left_[promoted] = node;
    parent_[node] = promoted;
    right_[node] = transferred;
    if (transferred != lzss_red_black_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

std::uint32_t LzssRedBlackTreeMatchFinder::rotate_right(
    const std::uint32_t node) noexcept {
    const auto promoted = left_[node];
    const auto transferred = right_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    right_[promoted] = node;
    parent_[node] = promoted;
    left_[node] = transferred;
    if (transferred != lzss_red_black_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

void LzssRedBlackTreeMatchFinder::repair_after_insertion(
    std::uint32_t node) noexcept {
    while (node != root_
           && node_color(parent_[node]) == LzssRedBlackTreeNodeColor::red) {
        auto parent = parent_[node];
        const auto grandparent = parent_[parent];
        if (parent == left_[grandparent]) {
            const auto uncle = right_[grandparent];
            if (node_color(uncle) == LzssRedBlackTreeNodeColor::red) {
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                color_[uncle] = LzssRedBlackTreeNodeColor::black;
                color_[grandparent] = LzssRedBlackTreeNodeColor::red;
                node = grandparent;
            } else {
                if (node == right_[parent]) {
                    node = parent;
                    static_cast<void>(rotate_left(node));
                }
                parent = parent_[node];
                const auto repaired_grandparent = parent_[parent];
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                color_[repaired_grandparent] =
                    LzssRedBlackTreeNodeColor::red;
                static_cast<void>(rotate_right(repaired_grandparent));
            }
        } else {
            const auto uncle = left_[grandparent];
            if (node_color(uncle) == LzssRedBlackTreeNodeColor::red) {
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                color_[uncle] = LzssRedBlackTreeNodeColor::black;
                color_[grandparent] = LzssRedBlackTreeNodeColor::red;
                node = grandparent;
            } else {
                if (node == left_[parent]) {
                    node = parent;
                    static_cast<void>(rotate_right(node));
                }
                parent = parent_[node];
                const auto repaired_grandparent = parent_[parent];
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                color_[repaired_grandparent] =
                    LzssRedBlackTreeNodeColor::red;
                static_cast<void>(rotate_left(repaired_grandparent));
            }
        }
    }
    color_[root_] = LzssRedBlackTreeNodeColor::black;
}

void LzssRedBlackTreeMatchFinder::repair_after_removal(
    std::uint32_t node, std::uint32_t parent) noexcept {
    while (node != root_
           && node_color(node) == LzssRedBlackTreeNodeColor::black) {
        if (parent == lzss_red_black_tree_null_node) break;
        if (node == left_[parent]) {
            auto sibling = right_[parent];
            if (node_color(sibling) == LzssRedBlackTreeNodeColor::red) {
                color_[sibling] = LzssRedBlackTreeNodeColor::black;
                color_[parent] = LzssRedBlackTreeNodeColor::red;
                static_cast<void>(rotate_left(parent));
                sibling = right_[parent];
            }
            if (sibling == lzss_red_black_tree_null_node) {
                node = parent;
                parent = parent_[node];
                continue;
            }
            if (node_color(left_[sibling])
                    == LzssRedBlackTreeNodeColor::black
                && node_color(right_[sibling])
                    == LzssRedBlackTreeNodeColor::black) {
                color_[sibling] = LzssRedBlackTreeNodeColor::red;
                node = parent;
                parent = parent_[node];
            } else {
                if (node_color(right_[sibling])
                    == LzssRedBlackTreeNodeColor::black) {
                    const auto near_child = left_[sibling];
                    if (near_child != lzss_red_black_tree_null_node) {
                        color_[near_child] =
                            LzssRedBlackTreeNodeColor::black;
                    }
                    color_[sibling] = LzssRedBlackTreeNodeColor::red;
                    static_cast<void>(rotate_right(sibling));
                    sibling = right_[parent];
                }
                color_[sibling] = color_[parent];
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                const auto far_child = right_[sibling];
                if (far_child != lzss_red_black_tree_null_node) {
                    color_[far_child] = LzssRedBlackTreeNodeColor::black;
                }
                static_cast<void>(rotate_left(parent));
                node = root_;
                parent = lzss_red_black_tree_null_node;
            }
        } else {
            auto sibling = left_[parent];
            if (node_color(sibling) == LzssRedBlackTreeNodeColor::red) {
                color_[sibling] = LzssRedBlackTreeNodeColor::black;
                color_[parent] = LzssRedBlackTreeNodeColor::red;
                static_cast<void>(rotate_right(parent));
                sibling = left_[parent];
            }
            if (sibling == lzss_red_black_tree_null_node) {
                node = parent;
                parent = parent_[node];
                continue;
            }
            if (node_color(right_[sibling])
                    == LzssRedBlackTreeNodeColor::black
                && node_color(left_[sibling])
                    == LzssRedBlackTreeNodeColor::black) {
                color_[sibling] = LzssRedBlackTreeNodeColor::red;
                node = parent;
                parent = parent_[node];
            } else {
                if (node_color(left_[sibling])
                    == LzssRedBlackTreeNodeColor::black) {
                    const auto near_child = right_[sibling];
                    if (near_child != lzss_red_black_tree_null_node) {
                        color_[near_child] =
                            LzssRedBlackTreeNodeColor::black;
                    }
                    color_[sibling] = LzssRedBlackTreeNodeColor::red;
                    static_cast<void>(rotate_left(sibling));
                    sibling = left_[parent];
                }
                color_[sibling] = color_[parent];
                color_[parent] = LzssRedBlackTreeNodeColor::black;
                const auto far_child = left_[sibling];
                if (far_child != lzss_red_black_tree_null_node) {
                    color_[far_child] = LzssRedBlackTreeNodeColor::black;
                }
                static_cast<void>(rotate_right(parent));
                node = root_;
                parent = lzss_red_black_tree_null_node;
            }
        }
    }
    if (node != lzss_red_black_tree_null_node) {
        color_[node] = LzssRedBlackTreeNodeColor::black;
    }
}

std::uint32_t LzssRedBlackTreeMatchFinder::minimum_node(
    std::uint32_t node) const noexcept {
    while (left_[node] != lzss_red_black_tree_null_node) {
        node = left_[node];
    }
    return node;
}

void LzssRedBlackTreeMatchFinder::clear_node(
    const std::uint32_t node) noexcept {
    left_[node] = lzss_red_black_tree_null_node;
    right_[node] = lzss_red_black_tree_null_node;
    parent_[node] = lzss_red_black_tree_null_node;
    color_[node] = LzssRedBlackTreeNodeColor::inactive;
    position_[node] = std::numeric_limits<std::size_t>::max();
    subtree_maximum_position_[node] =
        std::numeric_limits<std::size_t>::max();
}

LzssRedBlackTreeWorkspaceRequirements
calculate_lzss_red_black_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssRedBlackTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssRedBlackTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssRedBlackTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssRedBlackTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_red_black_tree_prefix_size) {
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
        || !append_array(result.node_count, sizeof(LzssRedBlackTreeNodeColor),
                         alignof(LzssRedBlackTreeNodeColor), cursor,
                         result.color_offset)
        || !append_array(result.node_count, sizeof(std::size_t),
                         alignof(std::size_t), cursor, result.position_offset)
        || !append_array(
            result.node_count, sizeof(std::size_t), alignof(std::size_t),
            cursor, result.subtree_maximum_position_offset)) {
        result.error = LzssRedBlackTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssRedBlackTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssRedBlackTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssRedBlackTreeError initialize_lzss_red_black_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssRedBlackTreeMatchFinder& finder) noexcept {
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssRedBlackTreeError::none) return required.error;
    if (workspace.size() < required.workspace_size) {
        return LzssRedBlackTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssRedBlackTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssRedBlackTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssRedBlackTreeError::arithmetic_overflow;
    }

    LzssRedBlackTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssRedBlackTreeError::none;
    }

    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.color_ = array_at<LzssRedBlackTreeNodeColor>(
        active_workspace, required.color_offset, required.node_count);
    initialized.position_ = array_at<std::size_t>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ = array_at<std::size_t>(
        active_workspace, required.subtree_maximum_position_offset,
        required.node_count);

    construct_array(initialized.left_, lzss_red_black_tree_null_node);
    construct_array(initialized.right_, lzss_red_black_tree_null_node);
    construct_array(initialized.parent_, lzss_red_black_tree_null_node);
    construct_array(initialized.color_, LzssRedBlackTreeNodeColor::inactive);
    construct_array(
        initialized.position_, std::numeric_limits<std::size_t>::max());
    construct_array(
        initialized.subtree_maximum_position_,
        std::numeric_limits<std::size_t>::max());

    finder = initialized;
    return LzssRedBlackTreeError::none;
}

LzssRedBlackTreeError insert_lzss_red_black_tree_position(
    LzssRedBlackTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || !finder.state_valid_
        || finder.left_.empty()) {
        return LzssRedBlackTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position
            < lzss_red_black_tree_prefix_size) {
        return LzssRedBlackTreeError::invalid_position;
    }
    const auto slot = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.color_[slot] != LzssRedBlackTreeNodeColor::inactive
        || finder.active_node_count_ == finder.left_.size()) {
        return LzssRedBlackTreeError::invalid_state;
    }

    auto parent = lzss_red_black_tree_null_node;
    auto current = finder.root_;
    int comparison{};
    std::size_t steps{};
    while (current != lzss_red_black_tree_null_node) {
        if (current >= finder.left_.size()
            || finder.color_[current]
                == LzssRedBlackTreeNodeColor::inactive
            || finder.position_[current] >= finder.input_.size()
            || finder.input_.size() - finder.position_[current]
                < lzss_red_black_tree_prefix_size
            || finder.position_[current] % finder.left_.size() != current
            || steps++ >= finder.active_node_count_) {
            return LzssRedBlackTreeError::invalid_state;
        }
        parent = current;
        comparison = finder.compare_positions(
            position, finder.position_[current]);
        current = comparison < 0 ? finder.left_[current]
                                 : finder.right_[current];
    }

    finder.left_[slot] = lzss_red_black_tree_null_node;
    finder.right_[slot] = lzss_red_black_tree_null_node;
    finder.parent_[slot] = parent;
    finder.color_[slot] = LzssRedBlackTreeNodeColor::red;
    finder.position_[slot] = position;
    finder.subtree_maximum_position_[slot] = position;
    if (parent == lzss_red_black_tree_null_node) {
        finder.root_ = slot;
    } else if (comparison < 0) {
        finder.left_[parent] = slot;
    } else {
        finder.right_[parent] = slot;
    }
    ++finder.active_node_count_;
    finder.update_metadata_upward(parent);
    finder.repair_after_insertion(slot);
    return LzssRedBlackTreeError::none;
}

LzssRedBlackTreeError remove_lzss_red_black_tree_position(
    LzssRedBlackTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || !finder.state_valid_
        || finder.left_.empty()) {
        return LzssRedBlackTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position
            < lzss_red_black_tree_prefix_size) {
        return LzssRedBlackTreeError::invalid_position;
    }
    const auto removed = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.color_[removed] == LzssRedBlackTreeNodeColor::inactive
        || finder.position_[removed] != position) {
        return LzssRedBlackTreeError::invalid_state;
    }

    auto replacement_source = removed;
    auto removed_color = finder.color_[replacement_source];
    auto replacement = lzss_red_black_tree_null_node;
    auto replacement_parent = lzss_red_black_tree_null_node;
    auto metadata_start = lzss_red_black_tree_null_node;

    if (finder.left_[removed] == lzss_red_black_tree_null_node) {
        replacement = finder.right_[removed];
        replacement_parent = finder.parent_[removed];
        metadata_start = replacement_parent;
        finder.replace_parent_child(
            finder.parent_[removed], removed, replacement);
    } else if (finder.right_[removed]
               == lzss_red_black_tree_null_node) {
        replacement = finder.left_[removed];
        replacement_parent = finder.parent_[removed];
        metadata_start = replacement_parent;
        finder.replace_parent_child(
            finder.parent_[removed], removed, replacement);
    } else {
        replacement_source = finder.minimum_node(finder.right_[removed]);
        removed_color = finder.color_[replacement_source];
        replacement = finder.right_[replacement_source];
        if (finder.parent_[replacement_source] == removed) {
            replacement_parent = replacement_source;
            metadata_start = replacement_source;
            if (replacement != lzss_red_black_tree_null_node) {
                finder.parent_[replacement] = replacement_source;
            }
        } else {
            replacement_parent = finder.parent_[replacement_source];
            metadata_start = replacement_parent;
            finder.replace_parent_child(
                replacement_parent, replacement_source, replacement);
            finder.right_[replacement_source] = finder.right_[removed];
            finder.parent_[finder.right_[replacement_source]] =
                replacement_source;
        }
        finder.replace_parent_child(
            finder.parent_[removed], removed, replacement_source);
        finder.left_[replacement_source] = finder.left_[removed];
        finder.parent_[finder.left_[replacement_source]] =
            replacement_source;
        finder.color_[replacement_source] = finder.color_[removed];
    }

    if (metadata_start != lzss_red_black_tree_null_node) {
        finder.update_metadata_upward(metadata_start);
    }
    if (removed_color == LzssRedBlackTreeNodeColor::black) {
        finder.repair_after_removal(replacement, replacement_parent);
    }
    finder.clear_node(removed);
    --finder.active_node_count_;
    return LzssRedBlackTreeError::none;
}

LzssRedBlackTreeNeighborQueryResult
LzssRedBlackTreeMatchFinder::find_neighbors(
    const std::size_t position) const noexcept {
    LzssRedBlackTreeNeighborQueryResult result{};
    if (!initialized_ || !state_valid_) {
        result.error = LzssRedBlackTreeError::invalid_state;
        return result;
    }
    if (position > input_.size()) {
        result.error = LzssRedBlackTreeError::invalid_position;
        return result;
    }
    if (position != next_position_) {
        result.error = LzssRedBlackTreeError::invalid_state;
        return result;
    }
    if (input_.size() - position < lzss_red_black_tree_prefix_size
        || root_ == lzss_red_black_tree_null_node) {
        return result;
    }

    auto predecessor = lzss_red_black_tree_null_node;
    auto successor = lzss_red_black_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_red_black_tree_null_node) {
        if (current >= left_.size()
            || color_[current] == LzssRedBlackTreeNodeColor::inactive
            || steps++ >= active_node_count_) {
            result.error = LzssRedBlackTreeError::invalid_state;
            return result;
        }
        const auto comparison = compare_positions(position, position_[current]);
        if (comparison == 0) {
            result.error = LzssRedBlackTreeError::invalid_state;
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

    if (predecessor != lzss_red_black_tree_null_node) {
        result.predecessor_position = position_[predecessor];
        result.predecessor_lcp = common_prefix_length(
            position, result.predecessor_position);
    }
    if (successor != lzss_red_black_tree_null_node) {
        result.successor_position = position_[successor];
        result.successor_lcp = common_prefix_length(
            position, result.successor_position);
    }
    result.maximum_lcp = std::max(
        result.predecessor_lcp, result.successor_lcp);
    return result;
}

LzssRedBlackTreeCandidateQueryResult
LzssRedBlackTreeMatchFinder::find_candidate(
    const std::size_t position) const noexcept {
    LzssRedBlackTreeCandidateQueryResult result{};
    const auto neighbors = find_neighbors(position);
    if (neighbors.error != LzssRedBlackTreeError::none) {
        result.error = neighbors.error;
        return result;
    }
    if (neighbors.maximum_lcp < parameters_.min_match_length) return result;

    auto split = lzss_red_black_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_red_black_tree_null_node) {
        if (steps++ >= active_node_count_) {
            result.error = LzssRedBlackTreeError::invalid_state;
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
    if (split == lzss_red_black_tree_null_node) {
        result.error = LzssRedBlackTreeError::invalid_state;
        return result;
    }

    auto maximum_position = position_[split];
    current = left_[split];
    steps = 0;
    while (current != lzss_red_black_tree_null_node) {
        if (steps++ >= active_node_count_) {
            result.error = LzssRedBlackTreeError::invalid_state;
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
            if (right_[current] != lzss_red_black_tree_null_node) {
                maximum_position = std::max(
                    maximum_position,
                    subtree_maximum_position_[right_[current]]);
            }
            current = left_[current];
        }
    }

    current = right_[split];
    steps = 0;
    while (current != lzss_red_black_tree_null_node) {
        if (steps++ >= active_node_count_) {
            result.error = LzssRedBlackTreeError::invalid_state;
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
            if (left_[current] != lzss_red_black_tree_null_node) {
                maximum_position = std::max(
                    maximum_position,
                    subtree_maximum_position_[left_[current]]);
            }
            current = right_[current];
        }
    }

    result.candidate_position = maximum_position;
    result.length = neighbors.maximum_lcp;
    return result;
}

LzssMatch LzssRedBlackTreeMatchFinder::find_match(
    const std::size_t position) const noexcept {
    const auto candidate = find_candidate(position);
    if (candidate.error != LzssRedBlackTreeError::none
        || candidate.candidate_position == lzss_red_black_tree_no_position
        || candidate.candidate_position >= position) {
        return {};
    }
    const auto distance = position - candidate.candidate_position;
    if (distance > std::numeric_limits<std::uint32_t>::max()) return {};
    return {static_cast<std::uint32_t>(distance), candidate.length};
}

void LzssRedBlackTreeMatchFinder::advance(
    const std::size_t position,
    const std::size_t next_position) noexcept {
    if (!initialized_ || !state_valid_ || position != next_position_
        || next_position < position || next_position > input_.size()) {
        state_valid_ = false;
        next_position_ = input_.size();
        return;
    }
    for (auto current = position; current < next_position; ++current) {
        if (current >= parameters_.window_size) {
            const auto expired = current - parameters_.window_size;
            if (input_.size() - expired
                    >= lzss_red_black_tree_prefix_size
                && remove_lzss_red_black_tree_position(*this, expired)
                    != LzssRedBlackTreeError::none) {
                state_valid_ = false;
                next_position_ = input_.size();
                return;
            }
        }
        if (input_.size() - current >= lzss_red_black_tree_prefix_size
            && insert_lzss_red_black_tree_position(*this, current)
                != LzssRedBlackTreeError::none) {
            state_valid_ = false;
            next_position_ = input_.size();
            return;
        }
    }
    next_position_ = next_position;
}

LzssRedBlackTreeValidationError validate_lzss_red_black_tree(
    const LzssRedBlackTreeMatchFinder& finder) noexcept {
    if (!finder.initialized_) {
        return LzssRedBlackTreeValidationError::uninitialized;
    }
    if (!finder.state_valid_) {
        return LzssRedBlackTreeValidationError::invalid_protocol_state;
    }
    const auto capacity = finder.left_.size();
    if (finder.right_.size() != capacity
        || finder.parent_.size() != capacity
        || finder.color_.size() != capacity
        || finder.position_.size() != capacity
        || finder.subtree_maximum_position_.size() != capacity
        || finder.active_node_count_ > capacity) {
        return LzssRedBlackTreeValidationError::invalid_active_count;
    }
    if ((finder.active_node_count_ == 0)
        != (finder.root_ == lzss_red_black_tree_null_node)) {
        return LzssRedBlackTreeValidationError::invalid_root;
    }
    if (finder.root_ != lzss_red_black_tree_null_node) {
        if (finder.root_ >= capacity
            || finder.parent_[finder.root_]
                != lzss_red_black_tree_null_node
            || finder.color_[finder.root_]
                != LzssRedBlackTreeNodeColor::black) {
            return LzssRedBlackTreeValidationError::invalid_root;
        }
    }

    std::size_t observed_active{};
    std::size_t expected_black_height{};
    bool has_expected_black_height{};
    for (std::size_t raw_node = 0; raw_node < capacity; ++raw_node) {
        const auto node = static_cast<std::uint32_t>(raw_node);
        if (finder.color_[node] == LzssRedBlackTreeNodeColor::inactive) {
            if (finder.left_[node] != lzss_red_black_tree_null_node
                || finder.right_[node] != lzss_red_black_tree_null_node
                || finder.parent_[node] != lzss_red_black_tree_null_node
                || finder.position_[node]
                    != std::numeric_limits<std::size_t>::max()
                || finder.subtree_maximum_position_[node]
                    != std::numeric_limits<std::size_t>::max()) {
                return LzssRedBlackTreeValidationError::invalid_inactive_node;
            }
            continue;
        }
        if (finder.color_[node] != LzssRedBlackTreeNodeColor::black
            && finder.color_[node] != LzssRedBlackTreeNodeColor::red) {
            return LzssRedBlackTreeValidationError::invalid_color;
        }
        ++observed_active;
        if (capacity == 0 || finder.position_[node] >= finder.input_.size()
            || finder.input_.size() - finder.position_[node]
                < lzss_red_black_tree_prefix_size
            || finder.position_[node] % capacity != node) {
            return LzssRedBlackTreeValidationError::invalid_slot_position;
        }

        const auto left = finder.left_[node];
        const auto right = finder.right_[node];
        for (const auto child : {left, right}) {
            if (child != lzss_red_black_tree_null_node
                && (child >= capacity
                    || finder.color_[child]
                        == LzssRedBlackTreeNodeColor::inactive)) {
                return LzssRedBlackTreeValidationError::invalid_index;
            }
            if (child != lzss_red_black_tree_null_node
                && (finder.position_[child] >= finder.input_.size()
                    || finder.input_.size() - finder.position_[child]
                        < lzss_red_black_tree_prefix_size
                    || finder.position_[child] % capacity != child)) {
                return LzssRedBlackTreeValidationError::invalid_slot_position;
            }
        }
        if ((left != lzss_red_black_tree_null_node
             && finder.parent_[left] != node)
            || (right != lzss_red_black_tree_null_node
                && finder.parent_[right] != node)) {
            return LzssRedBlackTreeValidationError::invalid_parent;
        }
        if ((left != lzss_red_black_tree_null_node
             && finder.compare_positions(
                    finder.position_[left], finder.position_[node]) >= 0)
            || (right != lzss_red_black_tree_null_node
                && finder.compare_positions(
                       finder.position_[right], finder.position_[node]) <= 0)) {
            return LzssRedBlackTreeValidationError::invalid_order;
        }
        if (finder.color_[node] == LzssRedBlackTreeNodeColor::red
            && (finder.node_color(left) == LzssRedBlackTreeNodeColor::red
                || finder.node_color(right)
                    == LzssRedBlackTreeNodeColor::red)) {
            return LzssRedBlackTreeValidationError::red_parent_violation;
        }

        auto expected_maximum = finder.position_[node];
        if (left != lzss_red_black_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[left]);
        }
        if (right != lzss_red_black_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[right]);
        }
        if (finder.subtree_maximum_position_[node] != expected_maximum) {
            return LzssRedBlackTreeValidationError::invalid_subtree_maximum;
        }

        auto ancestor = node;
        std::size_t steps{};
        std::size_t black_height{};
        while (true) {
            if (finder.color_[ancestor]
                == LzssRedBlackTreeNodeColor::black) {
                ++black_height;
            }
            if (ancestor == finder.root_) break;
            ancestor = finder.parent_[ancestor];
            if (ancestor == lzss_red_black_tree_null_node
                || ancestor >= capacity
                || finder.color_[ancestor]
                    == LzssRedBlackTreeNodeColor::inactive
                || ++steps > finder.active_node_count_) {
                return LzssRedBlackTreeValidationError::cycle_or_disconnected;
            }
        }
        if (left == lzss_red_black_tree_null_node
            || right == lzss_red_black_tree_null_node) {
            if (!has_expected_black_height) {
                expected_black_height = black_height;
                has_expected_black_height = true;
            } else if (black_height != expected_black_height) {
                return LzssRedBlackTreeValidationError::invalid_black_height;
            }
        }
    }
    if (observed_active != finder.active_node_count_) {
        return LzssRedBlackTreeValidationError::invalid_active_count;
    }
    return LzssRedBlackTreeValidationError::none;
}

LzssRedBlackTreeNodeSnapshot inspect_lzss_red_black_tree_node(
    const LzssRedBlackTreeMatchFinder& finder,
    const std::uint32_t node) noexcept {
    if (!finder.initialized_ || node >= finder.left_.size()) return {};
    return {finder.left_[node], finder.right_[node], finder.parent_[node],
            finder.color_[node], finder.position_[node],
            finder.subtree_maximum_position_[node]};
}

} // namespace marc::dictionary::internal

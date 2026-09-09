#include "dictionary/lzss_wavl_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

void increment_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value) noexcept {
    if (statistics == nullptr) return;
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics->overflowed = true;
        return;
    }
    ++value;
}

void add_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value, const std::uint64_t addend) noexcept {
    if (statistics == nullptr) return;
    if (addend > std::numeric_limits<std::uint64_t>::max() - value) {
        value = std::numeric_limits<std::uint64_t>::max();
        statistics->overflowed = true;
        return;
    }
    value += addend;
}

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

LzssWavlTreeError calculate_lzss_wavl_promoted_rank(
    const std::uint8_t rank, std::uint8_t& promoted_rank) noexcept {
    if (rank == lzss_wavl_tree_inactive_rank) {
        return LzssWavlTreeError::invalid_state;
    }
    if (rank == lzss_wavl_tree_inactive_rank - 1U) {
        return LzssWavlTreeError::rank_overflow;
    }
    promoted_rank = static_cast<std::uint8_t>(rank + 1U);
    return LzssWavlTreeError::none;
}

int LzssWavlTreeMatchFinder::node_rank(
    const std::uint32_t node) const noexcept {
    return node == lzss_wavl_tree_null_node ? -1
                                             : static_cast<int>(rank_[node]);
}

int LzssWavlTreeMatchFinder::compare_positions(
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

void LzssWavlTreeMatchFinder::update_metadata(
    const std::uint32_t node) noexcept {
    auto maximum_position = position_[node];
    if (left_[node] != lzss_wavl_tree_null_node) {
        maximum_position = std::max(
            maximum_position, subtree_maximum_position_[left_[node]]);
    }
    if (right_[node] != lzss_wavl_tree_null_node) {
        maximum_position = std::max(
            maximum_position, subtree_maximum_position_[right_[node]]);
    }
    subtree_maximum_position_[node] = maximum_position;
}

void LzssWavlTreeMatchFinder::update_metadata_upward(
    std::uint32_t node) noexcept {
    while (node != lzss_wavl_tree_null_node) {
        update_metadata(node);
        node = parent_[node];
    }
}

void LzssWavlTreeMatchFinder::replace_parent_child(
    const std::uint32_t parent, const std::uint32_t previous_child,
    const std::uint32_t replacement) noexcept {
    if (parent == lzss_wavl_tree_null_node) {
        root_ = replacement;
    } else if (left_[parent] == previous_child) {
        left_[parent] = replacement;
    } else {
        right_[parent] = replacement;
    }
    if (replacement != lzss_wavl_tree_null_node) {
        parent_[replacement] = parent;
    }
}

std::uint32_t LzssWavlTreeMatchFinder::rotate_left(
    const std::uint32_t node) noexcept {
    const auto promoted = right_[node];
    const auto transferred = left_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    left_[promoted] = node;
    parent_[node] = promoted;
    right_[node] = transferred;
    if (transferred != lzss_wavl_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

std::uint32_t LzssWavlTreeMatchFinder::rotate_right(
    const std::uint32_t node) noexcept {
    const auto promoted = left_[node];
    const auto transferred = right_[promoted];
    const auto parent = parent_[node];
    replace_parent_child(parent, node, promoted);
    right_[promoted] = node;
    parent_[node] = promoted;
    left_[node] = transferred;
    if (transferred != lzss_wavl_tree_null_node) {
        parent_[transferred] = node;
    }
    update_metadata(node);
    update_metadata(promoted);
    return promoted;
}

LzssWavlTreeError LzssWavlTreeMatchFinder::preflight_insertion_repair(
    std::uint32_t parent, const bool insert_left) const noexcept {
    if (parent == lzss_wavl_tree_null_node) {
        return LzssWavlTreeError::none;
    }

    const auto sibling = insert_left ? right_[parent] : left_[parent];
    const auto parent_rank = node_rank(parent);
    const auto sibling_difference = parent_rank - node_rank(sibling);
    if (parent_rank == 1) {
        return sibling_difference == 1 || sibling_difference == 2
            ? LzssWavlTreeError::none : LzssWavlTreeError::invalid_state;
    }
    if (parent_rank != 0 || sibling_difference != 1) {
        return LzssWavlTreeError::invalid_state;
    }

    auto child = parent;
    auto virtual_child_rank = 1;
    parent = parent_[child];
    std::size_t steps{};
    while (parent != lzss_wavl_tree_null_node) {
        if (parent >= left_.size()
            || rank_[parent] == lzss_wavl_tree_inactive_rank
            || (left_[parent] != child && right_[parent] != child)
            || steps++ >= active_node_count_) {
            return LzssWavlTreeError::invalid_state;
        }
        const auto current_rank = node_rank(parent);
        const auto child_difference = current_rank - virtual_child_rank;
        if (child_difference == 1 || child_difference == 2) {
            return LzssWavlTreeError::none;
        }
        if (child_difference != 0) {
            return LzssWavlTreeError::invalid_state;
        }

        const auto child_is_left = left_[parent] == child;
        const auto other = child_is_left ? right_[parent] : left_[parent];
        const auto other_difference = current_rank - node_rank(other);
        if (other_difference == 1) {
            auto promoted_rank = rank_[parent];
            const auto promotion = calculate_lzss_wavl_promoted_rank(
                rank_[parent], promoted_rank);
            if (promotion != LzssWavlTreeError::none) return promotion;
            child = parent;
            virtual_child_rank = promoted_rank;
            parent = parent_[child];
            continue;
        }
        if (other_difference != 2) {
            return LzssWavlTreeError::invalid_state;
        }

        const auto inner = child_is_left ? right_[child] : left_[child];
        const auto inner_difference = virtual_child_rank - node_rank(inner);
        if (inner_difference != 1 && inner_difference != 2) {
            return LzssWavlTreeError::invalid_state;
        }
        if (inner_difference == 1
            && (inner == lzss_wavl_tree_null_node
                || rank_[inner] == lzss_wavl_tree_inactive_rank)) {
            return LzssWavlTreeError::invalid_state;
        }
        return LzssWavlTreeError::none;
    }
    return LzssWavlTreeError::none;
}

void LzssWavlTreeMatchFinder::repair_after_insertion(
    std::uint32_t node) noexcept {
    std::uint64_t steps{};
    auto parent = parent_[node];
    while (parent != lzss_wavl_tree_null_node
           && node_rank(parent) == node_rank(node)) {
        ++steps;
        const auto node_is_left = left_[parent] == node;
        const auto sibling = node_is_left ? right_[parent] : left_[parent];
        const auto sibling_difference = node_rank(parent) - node_rank(sibling);
        if (sibling_difference == 1) {
            auto promoted_rank = rank_[parent];
            static_cast<void>(calculate_lzss_wavl_promoted_rank(
                rank_[parent], promoted_rank));
            rank_[parent] = promoted_rank;
            if (statistics_ != nullptr) {
                increment_statistic(
                    statistics_,
                    statistics_->wavl_tree_insertion_promotion_count);
            }
            node = parent;
            parent = parent_[node];
            continue;
        }

        const auto inner = node_is_left ? right_[node] : left_[node];
        const auto inner_difference = node_rank(node) - node_rank(inner);
        if (inner_difference == 2) {
            --rank_[parent];
            const auto repaired = node_is_left ? rotate_right(parent)
                                               : rotate_left(parent);
            update_metadata_upward(parent_[repaired]);
            if (statistics_ != nullptr) {
                increment_statistic(
                    statistics_,
                    statistics_->wavl_tree_insertion_single_rotation_count);
            }
        } else {
            const auto middle = inner;
            if (node_is_left) {
                static_cast<void>(rotate_left(node));
                static_cast<void>(rotate_right(parent));
            } else {
                static_cast<void>(rotate_right(node));
                static_cast<void>(rotate_left(parent));
            }
            ++rank_[middle];
            --rank_[node];
            --rank_[parent];
            update_metadata_upward(parent_[middle]);
            if (statistics_ != nullptr) {
                increment_statistic(
                    statistics_,
                    statistics_->wavl_tree_insertion_double_rotation_count);
            }
        }
        break;
    }
    if (statistics_ != nullptr) {
        add_statistic(
            statistics_, statistics_->wavl_tree_insertion_fixup_step_count,
            steps);
        statistics_->wavl_tree_maximum_insertion_fixup_steps = std::max(
            statistics_->wavl_tree_maximum_insertion_fixup_steps, steps);
    }
}

LzssWavlTreeWorkspaceRequirements calculate_lzss_wavl_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssWavlTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssWavlTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssWavlTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssWavlTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size >= lzss_wavl_tree_prefix_size) {
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
                         alignof(std::uint8_t), cursor, result.rank_offset)
        || !append_array(result.node_count, sizeof(std::size_t),
                         alignof(std::size_t), cursor, result.position_offset)
        || !append_array(
            result.node_count, sizeof(std::size_t), alignof(std::size_t),
            cursor, result.subtree_maximum_position_offset)) {
        result.error = LzssWavlTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssWavlTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssWavlTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssWavlTreeError initialize_lzss_wavl_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssWavlTreeMatchFinder& finder,
    LzssMatchFinderStatistics* const statistics) noexcept {
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssWavlTreeError::none) return required.error;
    if (workspace.size() < required.workspace_size) {
        return LzssWavlTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssWavlTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssWavlTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssWavlTreeError::arithmetic_overflow;
    }

    LzssWavlTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.statistics_ = statistics;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssWavlTreeError::none;
    }

    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.rank_ = array_at<std::uint8_t>(
        active_workspace, required.rank_offset, required.node_count);
    initialized.position_ = array_at<std::size_t>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ = array_at<std::size_t>(
        active_workspace, required.subtree_maximum_position_offset,
        required.node_count);

    construct_array(initialized.left_, lzss_wavl_tree_null_node);
    construct_array(initialized.right_, lzss_wavl_tree_null_node);
    construct_array(initialized.parent_, lzss_wavl_tree_null_node);
    construct_array(initialized.rank_, lzss_wavl_tree_inactive_rank);
    construct_array(initialized.position_, lzss_wavl_tree_no_position);
    construct_array(
        initialized.subtree_maximum_position_, lzss_wavl_tree_no_position);

    finder = initialized;
    return LzssWavlTreeError::none;
}

LzssWavlTreeError insert_lzss_wavl_tree_position(
    LzssWavlTreeMatchFinder& finder,
    const std::size_t position) noexcept {
    if (!finder.initialized_ || !finder.state_valid_
        || finder.left_.empty()) {
        return LzssWavlTreeError::invalid_state;
    }
    if (position >= finder.input_.size()
        || finder.input_.size() - position < lzss_wavl_tree_prefix_size) {
        return LzssWavlTreeError::invalid_position;
    }
    const auto slot = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.rank_[slot] != lzss_wavl_tree_inactive_rank
        || finder.active_node_count_ == finder.left_.size()) {
        return LzssWavlTreeError::invalid_state;
    }
    if ((finder.active_node_count_ == 0)
        != (finder.root_ == lzss_wavl_tree_null_node)) {
        return LzssWavlTreeError::invalid_state;
    }

    auto parent = lzss_wavl_tree_null_node;
    auto current = finder.root_;
    int comparison{};
    std::size_t steps{};
    while (current != lzss_wavl_tree_null_node) {
        if (current >= finder.left_.size()
            || finder.rank_[current] == lzss_wavl_tree_inactive_rank
            || finder.parent_[current] != parent
            || finder.position_[current] >= finder.input_.size()
            || finder.input_.size() - finder.position_[current]
                < lzss_wavl_tree_prefix_size
            || finder.position_[current] % finder.left_.size() != current
            || steps++ >= finder.active_node_count_) {
            return LzssWavlTreeError::invalid_state;
        }
        for (const auto child : {finder.left_[current], finder.right_[current]}) {
            if (child == lzss_wavl_tree_null_node) continue;
            if (child >= finder.left_.size()
                || finder.rank_[child] == lzss_wavl_tree_inactive_rank
                || finder.parent_[child] != current) {
                return LzssWavlTreeError::invalid_state;
            }
            const auto difference = finder.node_rank(current)
                - finder.node_rank(child);
            if (difference != 1 && difference != 2) {
                return LzssWavlTreeError::invalid_state;
            }
        }
        if (finder.left_[current] == lzss_wavl_tree_null_node
            && finder.right_[current] == lzss_wavl_tree_null_node
            && finder.rank_[current] != 0) {
            return LzssWavlTreeError::invalid_state;
        }
        parent = current;
        comparison = finder.compare_positions(
            position, finder.position_[current]);
        current = comparison < 0 ? finder.left_[current]
                                 : finder.right_[current];
    }

    const auto insert_left = comparison < 0;
    const auto preflight = finder.preflight_insertion_repair(
        parent, insert_left);
    if (preflight != LzssWavlTreeError::none) return preflight;

    finder.left_[slot] = lzss_wavl_tree_null_node;
    finder.right_[slot] = lzss_wavl_tree_null_node;
    finder.parent_[slot] = parent;
    finder.rank_[slot] = 0;
    finder.position_[slot] = position;
    finder.subtree_maximum_position_[slot] = position;
    if (parent == lzss_wavl_tree_null_node) {
        finder.root_ = slot;
    } else if (insert_left) {
        finder.left_[parent] = slot;
    } else {
        finder.right_[parent] = slot;
    }
    ++finder.active_node_count_;
    finder.update_metadata_upward(parent);
    finder.repair_after_insertion(slot);
    if (finder.statistics_ != nullptr) {
        increment_statistic(
            finder.statistics_, finder.statistics_->wavl_tree_insertion_count);
    }
    return LzssWavlTreeError::none;
}

LzssWavlTreeValidationError validate_lzss_wavl_tree(
    const LzssWavlTreeMatchFinder& finder) noexcept {
    if (!finder.initialized_) {
        return LzssWavlTreeValidationError::uninitialized;
    }
    if (!finder.state_valid_) {
        return LzssWavlTreeValidationError::invalid_protocol_state;
    }
    const auto capacity = finder.left_.size();
    if (finder.right_.size() != capacity
        || finder.parent_.size() != capacity
        || finder.rank_.size() != capacity
        || finder.position_.size() != capacity
        || finder.subtree_maximum_position_.size() != capacity) {
        return LzssWavlTreeValidationError::invalid_active_count;
    }
    if (finder.active_node_count_ > capacity) {
        return LzssWavlTreeValidationError::invalid_active_count;
    }
    if ((finder.active_node_count_ == 0)
        != (finder.root_ == lzss_wavl_tree_null_node)) {
        return LzssWavlTreeValidationError::invalid_root;
    }
    if (finder.root_ != lzss_wavl_tree_null_node) {
        if (finder.root_ >= capacity
            || finder.parent_[finder.root_]
                != lzss_wavl_tree_null_node) {
            return LzssWavlTreeValidationError::invalid_root;
        }
    }

    std::size_t observed_active{};
    for (std::size_t raw_node = 0; raw_node < capacity; ++raw_node) {
        const auto node = static_cast<std::uint32_t>(raw_node);
        if (finder.rank_[node] == lzss_wavl_tree_inactive_rank) {
            if (finder.left_[node] != lzss_wavl_tree_null_node
                || finder.right_[node] != lzss_wavl_tree_null_node
                || finder.parent_[node] != lzss_wavl_tree_null_node
                || finder.position_[node] != lzss_wavl_tree_no_position
                || finder.subtree_maximum_position_[node]
                    != lzss_wavl_tree_no_position) {
                return LzssWavlTreeValidationError::invalid_inactive_node;
            }
            continue;
        }
        ++observed_active;
        if (capacity == 0 || finder.position_[node] >= finder.input_.size()
            || finder.input_.size() - finder.position_[node]
                < lzss_wavl_tree_prefix_size
            || finder.position_[node] % capacity != node) {
            return LzssWavlTreeValidationError::invalid_slot_position;
        }

        const auto left = finder.left_[node];
        const auto right = finder.right_[node];
        for (const auto child : {left, right}) {
            if (child != lzss_wavl_tree_null_node
                && (child >= capacity
                    || finder.rank_[child]
                        == lzss_wavl_tree_inactive_rank)) {
                return LzssWavlTreeValidationError::invalid_index;
            }
            if (child != lzss_wavl_tree_null_node
                && finder.parent_[child] != node) {
                return LzssWavlTreeValidationError::invalid_parent;
            }
            const auto difference = finder.node_rank(node)
                - finder.node_rank(child);
            if (difference != 1 && difference != 2) {
                return LzssWavlTreeValidationError::invalid_rank_difference;
            }
        }
        if (left == lzss_wavl_tree_null_node
            && right == lzss_wavl_tree_null_node
            && finder.rank_[node] != 0) {
            return LzssWavlTreeValidationError::invalid_leaf_rank;
        }
        if ((left != lzss_wavl_tree_null_node
             && finder.compare_positions(
                    finder.position_[left], finder.position_[node]) >= 0)
            || (right != lzss_wavl_tree_null_node
                && finder.compare_positions(
                       finder.position_[right], finder.position_[node]) <= 0)) {
            return LzssWavlTreeValidationError::invalid_order;
        }

        auto expected_maximum = finder.position_[node];
        if (left != lzss_wavl_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[left]);
        }
        if (right != lzss_wavl_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                finder.subtree_maximum_position_[right]);
        }
        if (finder.subtree_maximum_position_[node] != expected_maximum) {
            return LzssWavlTreeValidationError::invalid_subtree_maximum;
        }

        auto ancestor = node;
        std::size_t steps{};
        while (ancestor != finder.root_
               && steps <= finder.active_node_count_) {
            ancestor = finder.parent_[ancestor];
            if (ancestor == lzss_wavl_tree_null_node || ancestor >= capacity
                || finder.rank_[ancestor]
                    == lzss_wavl_tree_inactive_rank) {
                return LzssWavlTreeValidationError::cycle_or_disconnected;
            }
            ++steps;
        }
        if (ancestor != finder.root_) {
            return LzssWavlTreeValidationError::cycle_or_disconnected;
        }
    }
    if (observed_active != finder.active_node_count_) {
        return LzssWavlTreeValidationError::invalid_active_count;
    }
    return LzssWavlTreeValidationError::none;
}

LzssWavlTreeNodeSnapshot inspect_lzss_wavl_tree_node(
    const LzssWavlTreeMatchFinder& finder,
    const std::uint32_t node) noexcept {
    if (!finder.initialized_ || node >= finder.left_.size()) return {};
    return {finder.left_[node], finder.right_[node], finder.parent_[node],
            finder.rank_[node], finder.position_[node],
            finder.subtree_maximum_position_[node]};
}

} // namespace marc::dictionary::internal

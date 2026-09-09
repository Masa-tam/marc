#include "dictionary/lzss_wavl_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <bit>
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

void record_wavl_tree_query(
    LzssMatchFinderStatistics* const statistics,
    const std::uint64_t nodes_visited) noexcept {
    if (statistics == nullptr) return;
    increment_statistic(statistics, statistics->query_count);
    statistics->wavl_tree_maximum_nodes_per_query = std::max(
        statistics->wavl_tree_maximum_nodes_per_query, nodes_visited);
    const auto raw_bin = nodes_visited == 0 ? 0U
        : std::bit_width(nodes_visited);
    const auto bin = std::min<std::size_t>(
        raw_bin, statistics->wavl_tree_query_depth_histogram.size() - 1U);
    increment_statistic(
        statistics, statistics->wavl_tree_query_depth_histogram[bin]);
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
    if (statistics_ != nullptr) {
        increment_statistic(
            statistics_, statistics_->wavl_tree_key_comparison_count);
    }
    const auto left_size = std::min<std::size_t>(
        input_.size() - left, parameters_.max_match_length);
    const auto right_size = std::min<std::size_t>(
        input_.size() - right, parameters_.max_match_length);
    const auto common_size = std::min(left_size, right_size);
    for (std::size_t index = 0; index < common_size; ++index) {
        if (statistics_ != nullptr) {
            increment_statistic(
                statistics_, statistics_->wavl_tree_key_byte_comparison_count);
        }
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

std::uint32_t LzssWavlTreeMatchFinder::common_prefix_length(
    const std::size_t left, const std::size_t right) const noexcept {
    const auto maximum = std::min({
        input_.size() - left, input_.size() - right,
        static_cast<std::size_t>(parameters_.max_match_length)});
    std::size_t length{};
    while (length < maximum) {
        if (statistics_ != nullptr) {
            increment_statistic(
                statistics_, statistics_->wavl_tree_lcp_byte_comparison_count);
        }
        if (input_[left + length] != input_[right + length]) break;
        ++length;
    }
    return static_cast<std::uint32_t>(length);
}

int LzssWavlTreeMatchFinder::compare_prefix(
    const std::size_t position, const std::size_t query_position,
    const std::uint32_t length) const noexcept {
    if (statistics_ != nullptr) {
        increment_statistic(
            statistics_, statistics_->wavl_tree_key_comparison_count);
        increment_statistic(
            statistics_, statistics_->wavl_tree_prefix_range_comparison_count);
    }
    for (std::size_t index = 0; index < length; ++index) {
        if (statistics_ != nullptr) {
            increment_statistic(
                statistics_, statistics_->wavl_tree_key_byte_comparison_count);
        }
        const auto byte = std::to_integer<std::uint8_t>(
            input_[position + index]);
        const auto query_byte = std::to_integer<std::uint8_t>(
            input_[query_position + index]);
        if (byte < query_byte) return -1;
        if (byte > query_byte) return 1;
    }
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

void LzssWavlTreeMatchFinder::clear_node(
    const std::uint32_t node) noexcept {
    left_[node] = lzss_wavl_tree_null_node;
    right_[node] = lzss_wavl_tree_null_node;
    parent_[node] = lzss_wavl_tree_null_node;
    rank_[node] = lzss_wavl_tree_inactive_rank;
    position_[node] = lzss_wavl_tree_no_position;
    subtree_maximum_position_[node] = lzss_wavl_tree_no_position;
}

void LzssWavlTreeMatchFinder::repair_after_removal(
    std::uint32_t parent, std::uint32_t replacement,
    bool replacement_is_left) noexcept {
    std::uint64_t steps{};
    const auto record_demotions = [this](const std::uint64_t count) noexcept {
        if (statistics_ != nullptr) {
            add_statistic(
                statistics_, statistics_->wavl_tree_removal_demotion_count,
                count);
        }
    };

    while (parent != lzss_wavl_tree_null_node) {
        const auto parent_rank = node_rank(parent);
        const auto transient_leaf = parent_rank == 1
            && left_[parent] == lzss_wavl_tree_null_node
            && right_[parent] == lzss_wavl_tree_null_node;
        const auto replacement_difference =
            parent_rank - node_rank(replacement);

        if (transient_leaf) {
            ++steps;
            --rank_[parent];
            record_demotions(1);
            replacement = parent;
            parent = parent_[replacement];
            if (parent != lzss_wavl_tree_null_node) {
                replacement_is_left = left_[parent] == replacement;
            }
            continue;
        }
        if (replacement_difference != 3) break;

        ++steps;
        const auto sibling = replacement_is_left
            ? right_[parent] : left_[parent];
        const auto sibling_difference = parent_rank - node_rank(sibling);
        if (sibling_difference == 2) {
            --rank_[parent];
            record_demotions(1);
            replacement = parent;
            parent = parent_[replacement];
            if (parent != lzss_wavl_tree_null_node) {
                replacement_is_left = left_[parent] == replacement;
            }
            continue;
        }

        const auto outer = replacement_is_left
            ? right_[sibling] : left_[sibling];
        const auto inner = replacement_is_left
            ? left_[sibling] : right_[sibling];
        const auto outer_difference = node_rank(sibling) - node_rank(outer);
        const auto inner_difference = node_rank(sibling) - node_rank(inner);
        if (outer_difference == 2 && inner_difference == 2) {
            --rank_[parent];
            --rank_[sibling];
            record_demotions(2);
            replacement = parent;
            parent = parent_[replacement];
            if (parent != lzss_wavl_tree_null_node) {
                replacement_is_left = left_[parent] == replacement;
            }
            continue;
        }

        if (outer_difference == 1) {
            if (inner == lzss_wavl_tree_null_node) {
                rank_[parent] = static_cast<std::uint8_t>(rank_[parent] - 2U);
                rank_[sibling] = static_cast<std::uint8_t>(rank_[sibling] + 1U);
                record_demotions(2);
            } else {
                --rank_[parent];
                ++rank_[sibling];
                record_demotions(1);
            }
            const auto repaired = replacement_is_left
                ? rotate_left(parent) : rotate_right(parent);
            update_metadata_upward(parent_[repaired]);
            if (statistics_ != nullptr) {
                increment_statistic(
                    statistics_,
                    statistics_->wavl_tree_removal_single_rotation_count);
            }
            break;
        }

        const auto middle = inner;
        rank_[middle] = static_cast<std::uint8_t>(rank_[middle] + 2U);
        --rank_[sibling];
        rank_[parent] = static_cast<std::uint8_t>(rank_[parent] - 2U);
        record_demotions(3);
        if (replacement_is_left) {
            static_cast<void>(rotate_right(sibling));
            static_cast<void>(rotate_left(parent));
        } else {
            static_cast<void>(rotate_left(sibling));
            static_cast<void>(rotate_right(parent));
        }
        update_metadata_upward(parent_[middle]);
        if (statistics_ != nullptr) {
            increment_statistic(
                statistics_,
                statistics_->wavl_tree_removal_double_rotation_count);
        }
        break;
    }

    if (statistics_ != nullptr) {
        add_statistic(
            statistics_, statistics_->wavl_tree_removal_fixup_step_count,
            steps);
        statistics_->wavl_tree_maximum_removal_fixup_steps = std::max(
            statistics_->wavl_tree_maximum_removal_fixup_steps, steps);
    }
}

LzssWavlTreeError LzssWavlTreeMatchFinder::preflight_removal(
    const std::uint32_t removed, std::uint64_t& nodes_visited) const noexcept {
    std::uint64_t visited{};
    const auto capacity = left_.size();
    const auto valid_active = [this, capacity](
                                  const std::uint32_t node) noexcept {
        return node < capacity
            && rank_[node] != lzss_wavl_tree_inactive_rank
            && position_[node] < input_.size()
            && input_.size() - position_[node]
                >= lzss_wavl_tree_prefix_size
            && position_[node] % capacity == node;
    };
    const auto validate_settled_node = [this, &valid_active](
                                           const std::uint32_t node) noexcept {
        if (!valid_active(node)) return false;
        auto expected_maximum = position_[node];
        for (const auto child : {left_[node], right_[node]}) {
            if (child == lzss_wavl_tree_null_node) continue;
            if (!valid_active(child) || parent_[child] != node) return false;
            const auto difference = node_rank(node) - node_rank(child);
            if (difference != 1 && difference != 2) return false;
            expected_maximum = std::max(
                expected_maximum, subtree_maximum_position_[child]);
        }
        if (left_[node] == lzss_wavl_tree_null_node
            && right_[node] == lzss_wavl_tree_null_node
            && rank_[node] != 0) {
            return false;
        }
        if ((left_[node] != lzss_wavl_tree_null_node
             && compare_positions(position_[left_[node]], position_[node])
                 >= 0)
            || (right_[node] != lzss_wavl_tree_null_node
                && compare_positions(
                       position_[right_[node]], position_[node]) <= 0)) {
            return false;
        }
        return subtree_maximum_position_[node] == expected_maximum;
    };

    const auto visit_settled_node = [&validate_settled_node, &visited](
                                        const std::uint32_t node) noexcept {
        if (visited != std::numeric_limits<std::uint64_t>::max()) ++visited;
        return validate_settled_node(node);
    };

    if (active_node_count_ == 0 || active_node_count_ > capacity
        || root_ == lzss_wavl_tree_null_node
        || !visit_settled_node(removed)) {
        return LzssWavlTreeError::invalid_state;
    }
    const auto removed_parent = parent_[removed];
    if ((removed_parent == lzss_wavl_tree_null_node && root_ != removed)
        || (removed_parent != lzss_wavl_tree_null_node
            && (!visit_settled_node(removed_parent)
                || (left_[removed_parent] != removed
                    && right_[removed_parent] != removed)))) {
        return LzssWavlTreeError::invalid_state;
    }

    const auto removed_left = left_[removed];
    const auto removed_right = right_[removed];
    auto successor = lzss_wavl_tree_null_node;
    auto successor_parent = lzss_wavl_tree_null_node;
    auto replacement = lzss_wavl_tree_null_node;
    auto repair_parent = removed_parent;
    auto replacement_is_left = removed_parent != lzss_wavl_tree_null_node
        && left_[removed_parent] == removed;
    auto direct_successor = false;

    if (removed_left == lzss_wavl_tree_null_node
        || removed_right == lzss_wavl_tree_null_node) {
        replacement = removed_left != lzss_wavl_tree_null_node
            ? removed_left : removed_right;
    } else {
        successor = removed_right;
        auto expected_parent = removed;
        std::size_t steps{};
        while (true) {
            if (!visit_settled_node(successor)
                || parent_[successor] != expected_parent
                || steps++ >= active_node_count_) {
                return LzssWavlTreeError::invalid_state;
            }
            if (left_[successor] == lzss_wavl_tree_null_node) break;
            expected_parent = successor;
            successor = left_[successor];
        }
        successor_parent = parent_[successor];
        replacement = right_[successor];
        direct_successor = successor_parent == removed;
        repair_parent = direct_successor ? successor : successor_parent;
        replacement_is_left = !direct_successor;
    }

    const auto virtual_rank = [this, successor, removed](
                                  const std::uint32_t node) noexcept {
        if (node == lzss_wavl_tree_null_node) return -1;
        return node == successor ? node_rank(removed) : node_rank(node);
    };
    const auto transplanted = successor == lzss_wavl_tree_null_node
        ? replacement : successor;
    const auto removed_was_left = removed_parent != lzss_wavl_tree_null_node
        && left_[removed_parent] == removed;
    const auto virtual_left = [this, successor, successor_parent,
                               removed_parent, removed_was_left,
                               removed_left, replacement, transplanted,
                               direct_successor](
                                  const std::uint32_t node) noexcept {
        if (node == successor) return removed_left;
        if (!direct_successor && node == successor_parent) return replacement;
        if (node == removed_parent && removed_was_left) return transplanted;
        return left_[node];
    };
    const auto virtual_right = [this, successor, removed_parent,
                                removed_was_left, removed_right, replacement,
                                transplanted, direct_successor](
                                   const std::uint32_t node) noexcept {
        if (node == successor) {
            return direct_successor ? replacement : removed_right;
        }
        if (node == removed_parent && !removed_was_left) return transplanted;
        return right_[node];
    };
    const auto virtual_parent = [this, successor, removed, removed_parent,
                                 removed_right, direct_successor](
                                    const std::uint32_t node) noexcept {
        if (node == successor) return removed_parent;
        if (!direct_successor && node == removed_right) return successor;
        return node == removed ? lzss_wavl_tree_null_node : parent_[node];
    };

    auto replacement_rank = virtual_rank(replacement);
    std::size_t repair_steps{};
    while (repair_parent != lzss_wavl_tree_null_node) {
        if (!visit_settled_node(repair_parent)
            || repair_steps++ >= active_node_count_) {
            return LzssWavlTreeError::invalid_state;
        }
        const auto expected_replacement = replacement_is_left
            ? virtual_left(repair_parent) : virtual_right(repair_parent);
        if (expected_replacement != replacement) {
            return LzssWavlTreeError::invalid_state;
        }
        const auto parent_rank = virtual_rank(repair_parent);
        const auto left = virtual_left(repair_parent);
        const auto right = virtual_right(repair_parent);
        const auto transient_leaf = parent_rank == 1
            && left == lzss_wavl_tree_null_node
            && right == lzss_wavl_tree_null_node;
        const auto replacement_difference = parent_rank - replacement_rank;
        if (transient_leaf) {
            replacement = repair_parent;
            replacement_rank = parent_rank - 1;
            repair_parent = virtual_parent(replacement);
            if (repair_parent != lzss_wavl_tree_null_node) {
                replacement_is_left =
                    virtual_left(repair_parent) == replacement;
            }
            continue;
        }
        if (replacement_difference == 1 || replacement_difference == 2) {
            nodes_visited = visited;
            return LzssWavlTreeError::none;
        }
        if (replacement_difference != 3) {
            return LzssWavlTreeError::invalid_state;
        }

        const auto sibling = replacement_is_left ? right : left;
        if (sibling == lzss_wavl_tree_null_node
            || !visit_settled_node(sibling)) {
            return LzssWavlTreeError::invalid_state;
        }
        const auto sibling_difference = parent_rank - virtual_rank(sibling);
        if (sibling_difference == 2) {
            replacement = repair_parent;
            replacement_rank = parent_rank - 1;
            repair_parent = virtual_parent(replacement);
            if (repair_parent != lzss_wavl_tree_null_node) {
                replacement_is_left =
                    virtual_left(repair_parent) == replacement;
            }
            continue;
        }
        if (sibling_difference != 1) {
            return LzssWavlTreeError::invalid_state;
        }

        const auto outer = replacement_is_left
            ? virtual_right(sibling) : virtual_left(sibling);
        const auto inner = replacement_is_left
            ? virtual_left(sibling) : virtual_right(sibling);
        const auto outer_difference =
            virtual_rank(sibling) - virtual_rank(outer);
        const auto inner_difference =
            virtual_rank(sibling) - virtual_rank(inner);
        if (outer_difference == 2 && inner_difference == 2) {
            replacement = repair_parent;
            replacement_rank = parent_rank - 1;
            repair_parent = virtual_parent(replacement);
            if (repair_parent != lzss_wavl_tree_null_node) {
                replacement_is_left =
                    virtual_left(repair_parent) == replacement;
            }
            continue;
        }
        if (outer_difference == 1) {
            if (inner == lzss_wavl_tree_null_node
                && (replacement != lzss_wavl_tree_null_node
                    || parent_rank != 2 || inner_difference != 2)) {
                return LzssWavlTreeError::invalid_state;
            }
            if (inner != lzss_wavl_tree_null_node
                && inner_difference != 1 && inner_difference != 2) {
                return LzssWavlTreeError::invalid_state;
            }
            nodes_visited = visited;
            return LzssWavlTreeError::none;
        }
        if (outer_difference == 2 && inner_difference == 1
            && inner != lzss_wavl_tree_null_node) {
            nodes_visited = visited;
            return LzssWavlTreeError::none;
        }
        return LzssWavlTreeError::invalid_state;
    }
    nodes_visited = visited;
    return LzssWavlTreeError::none;
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

LzssWavlTreeError remove_lzss_wavl_tree_position(
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
    const auto removed = static_cast<std::uint32_t>(
        position % finder.left_.size());
    if (finder.rank_[removed] == lzss_wavl_tree_inactive_rank
        || finder.position_[removed] != position) {
        return LzssWavlTreeError::invalid_state;
    }

    std::uint64_t preflight_nodes{};
    const auto preflight = finder.preflight_removal(
        removed, preflight_nodes);
    if (preflight != LzssWavlTreeError::none) return preflight;

    auto repair_parent = lzss_wavl_tree_null_node;
    auto replacement = lzss_wavl_tree_null_node;
    auto replacement_is_left = false;

    if (finder.left_[removed] == lzss_wavl_tree_null_node
        || finder.right_[removed] == lzss_wavl_tree_null_node) {
        replacement = finder.left_[removed] != lzss_wavl_tree_null_node
            ? finder.left_[removed] : finder.right_[removed];
        repair_parent = finder.parent_[removed];
        replacement_is_left = repair_parent != lzss_wavl_tree_null_node
            && finder.left_[repair_parent] == removed;
        finder.replace_parent_child(repair_parent, removed, replacement);
    } else {
        auto successor = finder.right_[removed];
        while (finder.left_[successor] != lzss_wavl_tree_null_node) {
            successor = finder.left_[successor];
        }
        const auto successor_parent = finder.parent_[successor];
        replacement = finder.right_[successor];
        if (successor_parent != removed) {
            repair_parent = successor_parent;
            replacement_is_left = true;
            finder.replace_parent_child(
                successor_parent, successor, replacement);
            finder.right_[successor] = finder.right_[removed];
            finder.parent_[finder.right_[successor]] = successor;
        } else {
            repair_parent = successor;
            replacement_is_left = false;
        }
        finder.replace_parent_child(
            finder.parent_[removed], removed, successor);
        finder.left_[successor] = finder.left_[removed];
        finder.parent_[finder.left_[successor]] = successor;
        finder.rank_[successor] = finder.rank_[removed];
    }

    finder.clear_node(removed);
    --finder.active_node_count_;
    finder.repair_after_removal(
        repair_parent, replacement, replacement_is_left);
    if (repair_parent != lzss_wavl_tree_null_node) {
        finder.update_metadata_upward(repair_parent);
    }
    if (finder.statistics_ != nullptr) {
        add_statistic(
            finder.statistics_,
            finder.statistics_->wavl_tree_removal_preflight_node_count,
            preflight_nodes);
        finder.statistics_->wavl_tree_maximum_removal_preflight_nodes =
            std::max(
                finder.statistics_->wavl_tree_maximum_removal_preflight_nodes,
                preflight_nodes);
        increment_statistic(
            finder.statistics_, finder.statistics_->wavl_tree_retirement_count);
    }
    return LzssWavlTreeError::none;
}

LzssWavlTreeNeighborQueryResult
LzssWavlTreeMatchFinder::find_neighbors(
    const std::size_t position) const noexcept {
    return find_neighbors_impl(position, nullptr);
}

LzssWavlTreeNeighborQueryResult
LzssWavlTreeMatchFinder::find_neighbors_impl(
    const std::size_t position,
    std::uint64_t* const nodes_visited) const noexcept {
    LzssWavlTreeNeighborQueryResult result{};
    if (!initialized_ || !state_valid_) {
        result.error = LzssWavlTreeError::invalid_state;
        return result;
    }
    if (position > input_.size()) {
        result.error = LzssWavlTreeError::invalid_position;
        return result;
    }
    if (position != next_position_) {
        result.error = LzssWavlTreeError::invalid_state;
        return result;
    }
    if (input_.size() - position < lzss_wavl_tree_prefix_size
        || root_ == lzss_wavl_tree_null_node) {
        return result;
    }

    auto predecessor = lzss_wavl_tree_null_node;
    auto successor = lzss_wavl_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_wavl_tree_null_node) {
        if (current >= left_.size()
            || rank_[current] == lzss_wavl_tree_inactive_rank
            || position_[current] >= input_.size()
            || input_.size() - position_[current]
                < lzss_wavl_tree_prefix_size
            || steps++ >= active_node_count_) {
            result.error = LzssWavlTreeError::invalid_state;
            return result;
        }
        if (nodes_visited != nullptr) ++*nodes_visited;
        const auto comparison = compare_positions(position, position_[current]);
        if (comparison == 0) {
            result.error = LzssWavlTreeError::invalid_state;
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

    if (predecessor != lzss_wavl_tree_null_node) {
        result.predecessor_position = position_[predecessor];
        result.predecessor_lcp = common_prefix_length(
            position, result.predecessor_position);
    }
    if (successor != lzss_wavl_tree_null_node) {
        result.successor_position = position_[successor];
        result.successor_lcp = common_prefix_length(
            position, result.successor_position);
    }
    result.maximum_lcp = std::max(
        result.predecessor_lcp, result.successor_lcp);
    return result;
}

LzssWavlTreeCandidateQueryResult
LzssWavlTreeMatchFinder::find_candidate(
    const std::size_t position) const noexcept {
    LzssWavlTreeCandidateQueryResult result{};
    std::uint64_t nodes_visited{};
    const auto finish = [this, &nodes_visited]() noexcept {
        record_wavl_tree_query(statistics_, nodes_visited);
    };
    const auto neighbors = find_neighbors_impl(position, &nodes_visited);
    if (neighbors.error != LzssWavlTreeError::none) {
        result.error = neighbors.error;
        finish();
        return result;
    }
    if (neighbors.maximum_lcp < parameters_.min_match_length) {
        finish();
        return result;
    }

    const auto valid_node = [this](const std::uint32_t node) noexcept {
        return node < left_.size()
            && rank_[node] != lzss_wavl_tree_inactive_rank
            && position_[node] < input_.size()
            && input_.size() - position_[node]
                >= lzss_wavl_tree_prefix_size;
    };
    auto split = lzss_wavl_tree_null_node;
    auto current = root_;
    std::size_t steps{};
    while (current != lzss_wavl_tree_null_node) {
        if (!valid_node(current) || steps++ >= active_node_count_) {
            result.error = LzssWavlTreeError::invalid_state;
            finish();
            return result;
        }
        ++nodes_visited;
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
    if (split == lzss_wavl_tree_null_node) {
        result.error = LzssWavlTreeError::invalid_state;
        finish();
        return result;
    }

    auto maximum_position = position_[split];
    current = left_[split];
    steps = 0;
    while (current != lzss_wavl_tree_null_node) {
        if (!valid_node(current) || steps++ >= active_node_count_) {
            result.error = LzssWavlTreeError::invalid_state;
            finish();
            return result;
        }
        ++nodes_visited;
        const auto comparison = compare_prefix(
            position_[current], position, neighbors.maximum_lcp);
        if (comparison < 0) {
            current = right_[current];
        } else if (comparison > 0) {
            current = left_[current];
        } else {
            maximum_position = std::max(maximum_position, position_[current]);
            const auto right = right_[current];
            if (right != lzss_wavl_tree_null_node) {
                if (!valid_node(right)) {
                    result.error = LzssWavlTreeError::invalid_state;
                    finish();
                    return result;
                }
                maximum_position = std::max(
                    maximum_position, subtree_maximum_position_[right]);
            }
            current = left_[current];
        }
    }

    current = right_[split];
    steps = 0;
    while (current != lzss_wavl_tree_null_node) {
        if (!valid_node(current) || steps++ >= active_node_count_) {
            result.error = LzssWavlTreeError::invalid_state;
            finish();
            return result;
        }
        ++nodes_visited;
        const auto comparison = compare_prefix(
            position_[current], position, neighbors.maximum_lcp);
        if (comparison < 0) {
            current = right_[current];
        } else if (comparison > 0) {
            current = left_[current];
        } else {
            maximum_position = std::max(maximum_position, position_[current]);
            const auto left = left_[current];
            if (left != lzss_wavl_tree_null_node) {
                if (!valid_node(left)) {
                    result.error = LzssWavlTreeError::invalid_state;
                    finish();
                    return result;
                }
                maximum_position = std::max(
                    maximum_position, subtree_maximum_position_[left]);
            }
            current = right_[current];
        }
    }

    if (maximum_position >= position) {
        result.error = LzssWavlTreeError::invalid_state;
        finish();
        return result;
    }
    result.candidate_position = maximum_position;
    result.length = neighbors.maximum_lcp;
    finish();
    return result;
}

LzssMatch LzssWavlTreeMatchFinder::find_match(
    const std::size_t position) const noexcept {
    const auto candidate = find_candidate(position);
    if (candidate.error != LzssWavlTreeError::none
        || candidate.candidate_position == lzss_wavl_tree_no_position
        || candidate.candidate_position >= position) {
        return {};
    }
    const auto distance = position - candidate.candidate_position;
    if (distance > std::numeric_limits<std::uint32_t>::max()) return {};
    return {static_cast<std::uint32_t>(distance), candidate.length};
}

void LzssWavlTreeMatchFinder::advance(
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
            if (input_.size() - expired >= lzss_wavl_tree_prefix_size
                && remove_lzss_wavl_tree_position(*this, expired)
                    != LzssWavlTreeError::none) {
                state_valid_ = false;
                next_position_ = input_.size();
                return;
            }
        }
        if (input_.size() - current >= lzss_wavl_tree_prefix_size
            && insert_lzss_wavl_tree_position(*this, current)
                != LzssWavlTreeError::none) {
            state_valid_ = false;
            next_position_ = input_.size();
            return;
        }
    }
    next_position_ = next_position;
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

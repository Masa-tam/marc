#include "dictionary/lzss_sparse_hash_tree_bucket_builder.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {
namespace {

void increment_statistic(
    LzssHashTreeComponentStatistics* const statistics,
    std::uint64_t& value) noexcept {
    if (statistics == nullptr) return;
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics->overflowed = true;
        return;
    }
    ++value;
}

void record_height(
    LzssHashTreeComponentStatistics* const statistics,
    const std::uint8_t height) noexcept {
    if (statistics == nullptr) return;
    statistics->maximum_height = std::max(
        statistics->maximum_height, static_cast<std::uint64_t>(height));
}

[[nodiscard]] LzssSparseHashTreeBucketBuildError validate_context(
    const LzssSparseHashTreeBucketBuildContext& context) noexcept {
    if (context.parameters.window_size == 0
        || context.parameters.max_match_length == 0
        || !lzss_hash_tree_position_extent_representable(
            context.input.size())) {
        return LzssSparseHashTreeBucketBuildError::invalid_parameters;
    }
    if (context.bucket_count == 0
        || !std::has_single_bit(context.bucket_count)
        || context.bucket >= context.bucket_count) {
        return LzssSparseHashTreeBucketBuildError::invalid_bucket;
    }
    if (context.query_position > context.input.size()) {
        return LzssSparseHashTreeBucketBuildError::invalid_query_position;
    }
    const auto expected_link_count = std::min<std::size_t>(
        context.input.size(), context.parameters.window_size);
    if (context.links.empty()
        || context.links.size() != expected_link_count) {
        return LzssSparseHashTreeBucketBuildError::invalid_link;
    }
    if (context.pool == nullptr || !context.pool->initialized()
        || !context.pool->state_valid()
        || context.pool->capacity() > context.links.size()) {
        return LzssSparseHashTreeBucketBuildError::invalid_pool;
    }
    const auto nodes = context.pool->node_arrays();
    const auto capacity = context.pool->capacity();
    if (nodes.left.size() != capacity || nodes.right.size() != capacity
        || nodes.parent.size() != capacity || nodes.height.size() != capacity
        || nodes.position.size() != capacity
        || nodes.subtree_maximum_position.size() != capacity) {
        return LzssSparseHashTreeBucketBuildError::invalid_pool;
    }
    return LzssSparseHashTreeBucketBuildError::none;
}

struct ChainInspection {
    std::size_t node_count{};
    LzssSparseHashTreeBucketBuildError error{
        LzssSparseHashTreeBucketBuildError::none};
};

[[nodiscard]] ChainInspection inspect_chain(
    const LzssSparseHashTreeBucketBuildContext& context) noexcept {
    ChainInspection result{};
    auto candidate = context.head_position;
    while (candidate != lzss_hash_tree_no_position) {
        if (candidate >= context.query_position
            || candidate >= context.input.size()
            || context.input.size() - candidate
                < lzss_match_finder_prefix_size) {
            result.error = LzssSparseHashTreeBucketBuildError::invalid_head;
            return result;
        }
        if (context.query_position - candidate
            > context.parameters.window_size) {
            break;
        }
        if (result.node_count == context.links.size()) {
            result.error = LzssSparseHashTreeBucketBuildError::invalid_link;
            return result;
        }
        const auto hash = calculate_lzss_prefix_hash(
            context.input, candidate);
        if (!hash.valid) {
            result.error = LzssSparseHashTreeBucketBuildError::invalid_head;
            return result;
        }
        if ((static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) != context.bucket) {
            result.error = LzssSparseHashTreeBucketBuildError::wrong_bucket;
            return result;
        }
        ++result.node_count;
        const auto distance = context.links[
            candidate % context.links.size()];
        if (distance == 0) break;
        if (distance > candidate) {
            result.error = LzssSparseHashTreeBucketBuildError::invalid_link;
            return result;
        }
        candidate -= distance;
    }
    return result;
}

[[nodiscard]] bool position_belongs_to_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    const std::size_t position) noexcept {
    if (position >= context.query_position
        || position >= context.input.size()
        || context.query_position - position
            > context.parameters.window_size
        || context.input.size() - position
            < lzss_match_finder_prefix_size) {
        return false;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    return hash.valid
        && (static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) == context.bucket;
}

[[nodiscard]] int compare_positions(
    const LzssSparseHashTreeBucketBuildContext& context,
    const std::size_t left, const std::size_t right) noexcept {
    const auto left_size = std::min<std::size_t>(
        context.input.size() - left,
        context.parameters.max_match_length);
    const auto right_size = std::min<std::size_t>(
        context.input.size() - right,
        context.parameters.max_match_length);
    const auto common_size = std::min(left_size, right_size);
    if (context.statistics != nullptr) {
        increment_statistic(
            context.statistics, context.statistics->key_comparison_count);
    }
    for (std::size_t index = 0; index < common_size; ++index) {
        if (context.statistics != nullptr) {
            increment_statistic(
                context.statistics,
                context.statistics->key_byte_comparison_count);
        }
        const auto left_byte = std::to_integer<std::uint8_t>(
            context.input[left + index]);
        const auto right_byte = std::to_integer<std::uint8_t>(
            context.input[right + index]);
        if (left_byte < right_byte) return -1;
        if (left_byte > right_byte) return 1;
    }
    if (left_size < right_size) return -1;
    if (left_size > right_size) return 1;
    if (left < right) return -1;
    if (left > right) return 1;
    return 0;
}

[[nodiscard]] bool valid_node_index(
    const std::uint32_t node, const std::size_t capacity) noexcept {
    return node == lzss_hash_tree_null_node || node < capacity;
}

[[nodiscard]] bool valid_active_node(
    const LzssSparseHashTreeBucketBuildContext& context,
    const LzssSparseHashTreeNodeArrays& nodes,
    const std::uint32_t node) noexcept {
    if (node >= nodes.left.size() || nodes.height[node] == 0
        || !position_belongs_to_bucket(context, nodes.position[node])
        || !valid_node_index(nodes.left[node], nodes.left.size())
        || !valid_node_index(nodes.right[node], nodes.left.size())) {
        return false;
    }
    return true;
}

[[nodiscard]] bool valid_local_node(
    const LzssSparseHashTreeBucketBuildContext& context,
    const LzssSparseHashTreeNodeArrays& nodes,
    const std::uint32_t node) noexcept {
    if (!valid_active_node(context, nodes, node)) return false;
    const auto left = nodes.left[node];
    const auto right = nodes.right[node];
    for (const auto child : {left, right}) {
        if (child == lzss_hash_tree_null_node) continue;
        if (child == node || !valid_active_node(context, nodes, child)
            || nodes.parent[child] != node) {
            return false;
        }
    }
    if (left != lzss_hash_tree_null_node
        && compare_positions(
               context, nodes.position[left], nodes.position[node]) >= 0) {
        return false;
    }
    if (right != lzss_hash_tree_null_node
        && compare_positions(
               context, nodes.position[right], nodes.position[node]) <= 0) {
        return false;
    }
    const auto left_height = left == lzss_hash_tree_null_node
        ? std::uint8_t{0} : nodes.height[left];
    const auto right_height = right == lzss_hash_tree_null_node
        ? std::uint8_t{0} : nodes.height[right];
    const auto expected_height = static_cast<std::uint8_t>(
        static_cast<unsigned>(std::max(left_height, right_height)) + 1U);
    const auto balance = static_cast<int>(left_height)
        - static_cast<int>(right_height);
    auto expected_maximum = static_cast<std::size_t>(nodes.position[node]);
    if (left != lzss_hash_tree_null_node) {
        expected_maximum = std::max(
            expected_maximum, static_cast<std::size_t>(
                nodes.subtree_maximum_position[left]));
    }
    if (right != lzss_hash_tree_null_node) {
        expected_maximum = std::max(
            expected_maximum, static_cast<std::size_t>(
                nodes.subtree_maximum_position[right]));
    }
    return nodes.height[node] == expected_height
        && balance >= -1 && balance <= 1
        && nodes.subtree_maximum_position[node] == expected_maximum;
}

class SparseBucketTreeBuilder {
public:
    SparseBucketTreeBuilder(
        const LzssSparseHashTreeBucketBuildContext& context,
        const LzssSparseHashTreeNodeArrays nodes) noexcept
        : context_(context), nodes_(nodes) {}

    [[nodiscard]] std::uint32_t root() const noexcept { return root_; }

    void insert(
        const std::size_t absolute_position,
        const std::uint32_t node) noexcept {
        nodes_.left[node] = lzss_hash_tree_null_node;
        nodes_.right[node] = lzss_hash_tree_null_node;
        nodes_.parent[node] = lzss_hash_tree_null_node;
        nodes_.height[node] = 1;
        nodes_.position[node] = static_cast<LzssHashTreeStoredPosition>(
            absolute_position);
        nodes_.subtree_maximum_position[node] =
            static_cast<LzssHashTreeStoredPosition>(absolute_position);
        if (root_ == lzss_hash_tree_null_node) {
            root_ = node;
            record_height(context_.statistics, std::uint8_t{1});
            return;
        }

        auto current = root_;
        auto parent = lzss_hash_tree_null_node;
        int order{};
        while (current != lzss_hash_tree_null_node) {
            parent = current;
            order = compare_positions(
                context_, absolute_position, nodes_.position[current]);
            current = order < 0 ? nodes_.left[current]
                                : nodes_.right[current];
        }
        nodes_.parent[node] = parent;
        if (order < 0) nodes_.left[parent] = node;
        else nodes_.right[parent] = node;
        rebalance_from(parent);
        record_height(context_.statistics, nodes_.height[root_]);
    }

private:
    [[nodiscard]] std::uint8_t height(
        const std::uint32_t node) const noexcept {
        return node == lzss_hash_tree_null_node
            ? std::uint8_t{0} : nodes_.height[node];
    }

    void update(const std::uint32_t node) noexcept {
        const auto left = nodes_.left[node];
        const auto right = nodes_.right[node];
        nodes_.height[node] = static_cast<std::uint8_t>(
            static_cast<unsigned>(std::max(height(left), height(right))) + 1U);
        auto maximum = static_cast<std::size_t>(nodes_.position[node]);
        if (left != lzss_hash_tree_null_node) {
            maximum = std::max(maximum, static_cast<std::size_t>(
                nodes_.subtree_maximum_position[left]));
        }
        if (right != lzss_hash_tree_null_node) {
            maximum = std::max(maximum, static_cast<std::size_t>(
                nodes_.subtree_maximum_position[right]));
        }
        nodes_.subtree_maximum_position[node] =
            static_cast<LzssHashTreeStoredPosition>(maximum);
    }

    void replace_parent_child(
        const std::uint32_t parent, const std::uint32_t previous,
        const std::uint32_t replacement) noexcept {
        if (parent == lzss_hash_tree_null_node) root_ = replacement;
        else if (nodes_.left[parent] == previous) {
            nodes_.left[parent] = replacement;
        } else {
            nodes_.right[parent] = replacement;
        }
        if (replacement != lzss_hash_tree_null_node) {
            nodes_.parent[replacement] = parent;
        }
    }

    [[nodiscard]] std::uint32_t rotate_left(
        const std::uint32_t node) noexcept {
        if (context_.statistics != nullptr) {
            increment_statistic(
                context_.statistics,
                context_.statistics->rotation_count);
        }
        const auto promoted = nodes_.right[node];
        const auto transferred = nodes_.left[promoted];
        const auto parent = nodes_.parent[node];
        replace_parent_child(parent, node, promoted);
        nodes_.left[promoted] = node;
        nodes_.parent[node] = promoted;
        nodes_.right[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            nodes_.parent[transferred] = node;
        }
        update(node);
        update(promoted);
        return promoted;
    }

    [[nodiscard]] std::uint32_t rotate_right(
        const std::uint32_t node) noexcept {
        if (context_.statistics != nullptr) {
            increment_statistic(
                context_.statistics,
                context_.statistics->rotation_count);
        }
        const auto promoted = nodes_.left[node];
        const auto transferred = nodes_.right[promoted];
        const auto parent = nodes_.parent[node];
        replace_parent_child(parent, node, promoted);
        nodes_.right[promoted] = node;
        nodes_.parent[node] = promoted;
        nodes_.left[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            nodes_.parent[transferred] = node;
        }
        update(node);
        update(promoted);
        return promoted;
    }

    [[nodiscard]] int balance_factor(
        const std::uint32_t node) const noexcept {
        return static_cast<int>(height(nodes_.left[node]))
            - static_cast<int>(height(nodes_.right[node]));
    }

    void rebalance_from(std::uint32_t node) noexcept {
        while (node != lzss_hash_tree_null_node) {
            update(node);
            auto subtree_root = node;
            const auto balance = balance_factor(node);
            if (balance == 2) {
                const auto left = nodes_.left[node];
                if (balance_factor(left) < 0) {
                    static_cast<void>(rotate_left(left));
                }
                subtree_root = rotate_right(node);
            } else if (balance == -2) {
                const auto right = nodes_.right[node];
                if (balance_factor(right) > 0) {
                    static_cast<void>(rotate_right(right));
                }
                subtree_root = rotate_left(node);
            }
            node = nodes_.parent[subtree_root];
        }
    }

    const LzssSparseHashTreeBucketBuildContext& context_;
    LzssSparseHashTreeNodeArrays nodes_{};
    std::uint32_t root_{lzss_hash_tree_null_node};
};

[[nodiscard]] bool tree_contains_position(
    const LzssSparseHashTreeBucketBuildContext& context,
    const LzssSparseHashTreeNodeArrays& nodes,
    const std::uint32_t root, const std::size_t position,
    const std::size_t expected_node_count) noexcept {
    auto current = root;
    std::size_t visited{};
    while (current != lzss_hash_tree_null_node) {
        if (visited++ == expected_node_count
            || !valid_local_node(context, nodes, current)) {
            return false;
        }
        const auto order = compare_positions(
            context, position, nodes.position[current]);
        if (order == 0) return true;
        current = order < 0 ? nodes.left[current] : nodes.right[current];
    }
    return false;
}

[[nodiscard]] bool validate_reachable_tree(
    const LzssSparseHashTreeBucketBuildContext& context,
    const LzssSparseHashTreeNodeArrays& nodes,
    const std::uint32_t root,
    const std::size_t expected_node_count) noexcept {
    auto previous = lzss_hash_tree_null_node;
    auto current = root;
    std::size_t entered_nodes{};
    std::size_t transitions{};
    while (current != lzss_hash_tree_null_node) {
        if (transitions++ > expected_node_count * 3U
            || current >= nodes.left.size()) {
            return false;
        }
        std::uint32_t next{};
        if (previous == nodes.parent[current]) {
            if (!valid_local_node(context, nodes, current)
                || ++entered_nodes > expected_node_count) {
                return false;
            }
            if (nodes.left[current] != lzss_hash_tree_null_node) {
                next = nodes.left[current];
            } else if (nodes.right[current] != lzss_hash_tree_null_node) {
                next = nodes.right[current];
            } else {
                next = nodes.parent[current];
            }
        } else if (previous == nodes.left[current]) {
            next = nodes.right[current] != lzss_hash_tree_null_node
                ? nodes.right[current] : nodes.parent[current];
        } else if (previous == nodes.right[current]) {
            next = nodes.parent[current];
        } else {
            return false;
        }
        previous = current;
        current = next;
    }
    return entered_nodes == expected_node_count;
}

[[nodiscard]] bool release_private_tree(
    LzssSparseHashTreeNodePool& pool, std::uint32_t root,
    const std::size_t expected_node_count) noexcept {
    auto nodes = pool.node_arrays();
    std::size_t released{};
    while (root != lzss_hash_tree_null_node) {
        if (released == expected_node_count) return false;
        auto leaf = root;
        std::size_t descent{};
        while (nodes.left[leaf] != lzss_hash_tree_null_node
               || nodes.right[leaf] != lzss_hash_tree_null_node) {
            if (descent++ == expected_node_count) return false;
            leaf = nodes.left[leaf] != lzss_hash_tree_null_node
                ? nodes.left[leaf] : nodes.right[leaf];
            if (leaf >= nodes.left.size()) return false;
        }
        const auto parent = nodes.parent[leaf];
        if (parent == lzss_hash_tree_null_node) {
            if (leaf != root) return false;
            root = lzss_hash_tree_null_node;
        } else if (parent >= nodes.left.size()) {
            return false;
        } else if (nodes.left[parent] == leaf) {
            nodes.left[parent] = lzss_hash_tree_null_node;
        } else if (nodes.right[parent] == leaf) {
            nodes.right[parent] = lzss_hash_tree_null_node;
        } else {
            return false;
        }
        if (pool.release(leaf) != LzssSparseHashTreeError::none) {
            return false;
        }
        ++released;
    }
    return released == expected_node_count;
}

} // namespace

LzssSparseHashTreeBucketBuildError validate_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    const std::uint32_t root,
    const std::size_t expected_node_count) noexcept {
    const auto context_error = validate_context(context);
    if (context_error != LzssSparseHashTreeBucketBuildError::none) {
        return context_error;
    }
    const auto chain = inspect_chain(context);
    if (chain.error != LzssSparseHashTreeBucketBuildError::none) {
        return chain.error;
    }
    if (chain.node_count != expected_node_count) {
        return LzssSparseHashTreeBucketBuildError::invalid_tree;
    }
    if (expected_node_count == 0) {
        return root == lzss_hash_tree_null_node
            ? LzssSparseHashTreeBucketBuildError::none
            : LzssSparseHashTreeBucketBuildError::invalid_tree;
    }
    const auto nodes = context.pool->node_arrays();
    if (root >= nodes.left.size()
        || nodes.parent[root] != lzss_hash_tree_null_node
        || !validate_reachable_tree(
            context, nodes, root, expected_node_count)) {
        return LzssSparseHashTreeBucketBuildError::invalid_tree;
    }

    auto candidate = context.head_position;
    for (std::size_t index = 0; index < expected_node_count; ++index) {
        if (!tree_contains_position(
                context, nodes, root, candidate, expected_node_count)) {
            return LzssSparseHashTreeBucketBuildError::invalid_tree;
        }
        const auto distance = context.links[
            candidate % context.links.size()];
        if (distance == 0) break;
        candidate -= distance;
    }
    return LzssSparseHashTreeBucketBuildError::none;
}

LzssSparseHashTreeBucketBuildError release_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    const std::uint32_t root,
    const std::size_t expected_node_count) noexcept {
    const auto validation = validate_lzss_sparse_hash_tree_bucket(
        context, root, expected_node_count);
    if (validation != LzssSparseHashTreeBucketBuildError::none) {
        return validation;
    }
    if (!release_private_tree(
            *context.pool, root, expected_node_count)) {
        return LzssSparseHashTreeBucketBuildError::pool_failure;
    }
    return LzssSparseHashTreeBucketBuildError::none;
}

LzssSparseHashTreeBucketBuildResult build_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context) noexcept {
    LzssSparseHashTreeBucketBuildResult result{};
    result.error = validate_context(context);
    if (result.error != LzssSparseHashTreeBucketBuildError::none) {
        return result;
    }
    const auto chain = inspect_chain(context);
    if (chain.error != LzssSparseHashTreeBucketBuildError::none) {
        result.error = chain.error;
        return result;
    }
    if (chain.node_count == 0) return result;
    if (chain.node_count > context.pool->free_count()) {
        result.status =
            LzssSparseHashTreeBucketBuildStatus::insufficient_capacity;
        return result;
    }

    const auto nodes = context.pool->node_arrays();
    SparseBucketTreeBuilder builder{context, nodes};
    auto candidate = context.head_position;
    std::size_t inserted{};
    for (std::size_t index = 0; index < chain.node_count; ++index) {
        const auto allocation = context.pool->allocate();
        if (!allocation.allocated
            || allocation.error != LzssSparseHashTreeError::none) {
            if (!release_private_tree(
                    *context.pool, builder.root(), inserted)) {
                result.error =
                    LzssSparseHashTreeBucketBuildError::pool_failure;
                return result;
            }
            result.error = LzssSparseHashTreeBucketBuildError::pool_failure;
            return result;
        }
        builder.insert(candidate, allocation.node);
        ++inserted;
        if (index + 1U < chain.node_count) {
            const auto distance = context.links[
                candidate % context.links.size()];
            candidate -= distance;
        }
    }
    const auto root = builder.root();
    result.error = validate_lzss_sparse_hash_tree_bucket(
        context, root, inserted);
    if (result.error != LzssSparseHashTreeBucketBuildError::none) {
        if (!release_private_tree(
                *context.pool, root, inserted)) {
            result.error = LzssSparseHashTreeBucketBuildError::pool_failure;
        }
        return result;
    }
    result.root = root;
    result.node_count = inserted;
    result.status = LzssSparseHashTreeBucketBuildStatus::built;
    return result;
}

} // namespace marc::dictionary::internal

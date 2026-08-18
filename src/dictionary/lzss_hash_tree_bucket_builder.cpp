#include "dictionary/lzss_hash_tree_bucket_builder.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

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

[[nodiscard]] LzssHashTreeBucketBuildError validate_context(
    const LzssHashTreeBucketBuildContext& context) noexcept {
    const auto capacity = context.links.size();
    if (context.parameters.window_size == 0
        || context.parameters.max_match_length == 0) {
        return LzssHashTreeBucketBuildError::invalid_parameters;
    }
    if (context.bucket_count == 0
        || !std::has_single_bit(context.bucket_count)
        || context.bucket >= context.bucket_count) {
        return LzssHashTreeBucketBuildError::invalid_bucket;
    }
    if (!lzss_hash_tree_position_extent_representable(context.input.size())) {
        return LzssHashTreeBucketBuildError::invalid_parameters;
    }
    if (context.query_position > context.input.size()) {
        return LzssHashTreeBucketBuildError::invalid_query_position;
    }
    const auto expected_capacity = std::min<std::size_t>(
        context.input.size(), context.parameters.window_size);
    if (capacity == 0 || capacity != expected_capacity
        || context.nodes.left.size() != capacity
        || context.nodes.right.size() != capacity
        || context.nodes.parent.size() != capacity
        || context.nodes.height.size() != capacity
        || context.nodes.position.size() != capacity
        || context.nodes.subtree_maximum_position.size() != capacity) {
        return LzssHashTreeBucketBuildError::invalid_node_arrays;
    }
    return LzssHashTreeBucketBuildError::none;
}

[[nodiscard]] int compare_positions(
    const LzssHashTreeBucketBuildContext& context,
    const std::size_t left, const std::size_t right) noexcept {
    const auto left_size = std::min<std::size_t>(
        context.input.size() - left, context.parameters.max_match_length);
    const auto right_size = std::min<std::size_t>(
        context.input.size() - right, context.parameters.max_match_length);
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

struct ChainInspection {
    std::size_t node_count{};
    LzssHashTreeBucketBuildError error{
        LzssHashTreeBucketBuildError::none};
};

[[nodiscard]] ChainInspection inspect_chain(
    const LzssHashTreeBucketBuildContext& context) noexcept {
    ChainInspection result{};
    auto candidate = context.head_position;
    while (candidate != lzss_hash_tree_no_position) {
        if (candidate >= context.query_position
            || candidate >= context.input.size()
            || context.input.size() - candidate
                < lzss_match_finder_prefix_size) {
            result.error = LzssHashTreeBucketBuildError::invalid_head;
            return result;
        }
        if (context.query_position - candidate
            > context.parameters.window_size) {
            break;
        }
        if (result.node_count == context.links.size()) {
            result.error = LzssHashTreeBucketBuildError::invalid_link;
            return result;
        }
        const auto hash = calculate_lzss_prefix_hash(
            context.input, candidate);
        if (!hash.valid) {
            result.error = LzssHashTreeBucketBuildError::invalid_head;
            return result;
        }
        const auto bucket = static_cast<std::size_t>(hash.value)
            & (context.bucket_count - 1U);
        if (bucket != context.bucket) {
            result.error = LzssHashTreeBucketBuildError::wrong_bucket;
            return result;
        }
        ++result.node_count;
        const auto distance = context.links[candidate % context.links.size()];
        if (distance == 0) break;
        if (distance > candidate) {
            result.error = LzssHashTreeBucketBuildError::invalid_link;
            return result;
        }
        candidate -= distance;
    }
    return result;
}

class BucketTreeBuilder {
public:
    explicit BucketTreeBuilder(
        const LzssHashTreeBucketBuildContext& context) noexcept
        : context_(context) {}

    [[nodiscard]] std::uint32_t root() const noexcept { return root_; }

    void insert(const std::size_t absolute_position) noexcept {
        const auto node = static_cast<std::uint32_t>(
            absolute_position % context_.links.size());
        std::construct_at(context_.nodes.left.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.nodes.right.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.nodes.parent.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.nodes.height.data() + node,
                          std::uint8_t{1});
        std::construct_at(context_.nodes.position.data() + node,
                          static_cast<LzssHashTreeStoredPosition>(
                              absolute_position));
        std::construct_at(
            context_.nodes.subtree_maximum_position.data() + node,
            static_cast<LzssHashTreeStoredPosition>(absolute_position));
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
                context_, absolute_position,
                context_.nodes.position[current]);
            current = order < 0 ? context_.nodes.left[current]
                                : context_.nodes.right[current];
        }
        context_.nodes.parent[node] = parent;
        if (order < 0) context_.nodes.left[parent] = node;
        else context_.nodes.right[parent] = node;
        rebalance_from(parent);
        record_height(context_.statistics, context_.nodes.height[root_]);
    }

private:
    [[nodiscard]] std::uint8_t height(
        const std::uint32_t node) const noexcept {
        return node == lzss_hash_tree_null_node ? std::uint8_t{0}
                                                : context_.nodes.height[node];
    }

    void update(const std::uint32_t node) noexcept {
        const auto left = context_.nodes.left[node];
        const auto right = context_.nodes.right[node];
        context_.nodes.height[node] = static_cast<std::uint8_t>(
            static_cast<unsigned>(std::max(height(left), height(right))) + 1U);
        auto maximum = static_cast<std::size_t>(
            context_.nodes.position[node]);
        if (left != lzss_hash_tree_null_node) {
            maximum = std::max(
                maximum,
                static_cast<std::size_t>(
                    context_.nodes.subtree_maximum_position[left]));
        }
        if (right != lzss_hash_tree_null_node) {
            maximum = std::max(
                maximum,
                static_cast<std::size_t>(
                    context_.nodes.subtree_maximum_position[right]));
        }
        context_.nodes.subtree_maximum_position[node] =
            static_cast<LzssHashTreeStoredPosition>(maximum);
    }

    void replace_parent_child(
        const std::uint32_t parent, const std::uint32_t previous,
        const std::uint32_t replacement) noexcept {
        if (parent == lzss_hash_tree_null_node) root_ = replacement;
        else if (context_.nodes.left[parent] == previous) {
            context_.nodes.left[parent] = replacement;
        } else {
            context_.nodes.right[parent] = replacement;
        }
        if (replacement != lzss_hash_tree_null_node) {
            context_.nodes.parent[replacement] = parent;
        }
    }

    [[nodiscard]] std::uint32_t rotate_left(
        const std::uint32_t node) noexcept {
        if (context_.statistics != nullptr) {
            increment_statistic(
                context_.statistics,
                context_.statistics->rotation_count);
        }
        const auto promoted = context_.nodes.right[node];
        const auto transferred = context_.nodes.left[promoted];
        const auto parent = context_.nodes.parent[node];
        replace_parent_child(parent, node, promoted);
        context_.nodes.left[promoted] = node;
        context_.nodes.parent[node] = promoted;
        context_.nodes.right[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            context_.nodes.parent[transferred] = node;
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
        const auto promoted = context_.nodes.left[node];
        const auto transferred = context_.nodes.right[promoted];
        const auto parent = context_.nodes.parent[node];
        replace_parent_child(parent, node, promoted);
        context_.nodes.right[promoted] = node;
        context_.nodes.parent[node] = promoted;
        context_.nodes.left[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            context_.nodes.parent[transferred] = node;
        }
        update(node);
        update(promoted);
        return promoted;
    }

    [[nodiscard]] int balance_factor(
        const std::uint32_t node) const noexcept {
        return static_cast<int>(height(context_.nodes.left[node]))
            - static_cast<int>(height(context_.nodes.right[node]));
    }

    void rebalance_from(std::uint32_t node) noexcept {
        while (node != lzss_hash_tree_null_node) {
            update(node);
            auto subtree_root = node;
            const auto balance = balance_factor(node);
            if (balance == 2) {
                const auto left = context_.nodes.left[node];
                if (balance_factor(left) < 0) {
                    static_cast<void>(rotate_left(left));
                }
                subtree_root = rotate_right(node);
            } else if (balance == -2) {
                const auto right = context_.nodes.right[node];
                if (balance_factor(right) > 0) {
                    static_cast<void>(rotate_right(right));
                }
                subtree_root = rotate_left(node);
            }
            node = context_.nodes.parent[subtree_root];
        }
    }

    const LzssHashTreeBucketBuildContext& context_;
    std::uint32_t root_{lzss_hash_tree_null_node};
};

[[nodiscard]] bool valid_node_index(
    const std::uint32_t node, const std::size_t capacity) noexcept {
    return node == lzss_hash_tree_null_node || node < capacity;
}

[[nodiscard]] bool is_active_bucket_position(
    const LzssHashTreeBucketBuildContext& context,
    const std::uint32_t node,
    const std::size_t absolute_position) noexcept {
    if (absolute_position >= context.query_position
        || absolute_position >= context.input.size()
        || context.query_position - absolute_position
            > context.parameters.window_size
        || context.input.size() - absolute_position
            < lzss_match_finder_prefix_size
        || absolute_position % context.links.size() != node) {
        return false;
    }
    const auto hash = calculate_lzss_prefix_hash(
        context.input, absolute_position);
    return hash.valid
        && (static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) == context.bucket;
}

} // namespace

LzssHashTreeBucketBuildError validate_lzss_hash_tree_bucket(
    const LzssHashTreeBucketBuildContext& context,
    const std::uint32_t root,
    const std::size_t expected_node_count) noexcept {
    const auto context_error = validate_context(context);
    if (context_error != LzssHashTreeBucketBuildError::none) {
        return context_error;
    }
    const auto chain = inspect_chain(context);
    if (chain.error != LzssHashTreeBucketBuildError::none) {
        return chain.error;
    }
    if (chain.node_count != expected_node_count) {
        return LzssHashTreeBucketBuildError::invalid_tree;
    }
    if (expected_node_count == 0) {
        return root == lzss_hash_tree_null_node
            ? LzssHashTreeBucketBuildError::none
            : LzssHashTreeBucketBuildError::invalid_tree;
    }
    const auto capacity = context.links.size();
    if (root >= capacity
        || !is_active_bucket_position(
            context, root, context.nodes.position[root])
        || context.nodes.parent[root] != lzss_hash_tree_null_node) {
        return LzssHashTreeBucketBuildError::invalid_tree;
    }

    std::size_t child_edge_count{};
    auto candidate = context.head_position;
    for (std::size_t visited = 0; visited < expected_node_count; ++visited) {
        const auto node = static_cast<std::uint32_t>(candidate % capacity);
        if (context.nodes.position[node] != candidate) {
            return LzssHashTreeBucketBuildError::invalid_tree;
        }
        const auto left = context.nodes.left[node];
        const auto right = context.nodes.right[node];
        if (!valid_node_index(left, capacity)
            || !valid_node_index(right, capacity)) {
            return LzssHashTreeBucketBuildError::invalid_tree;
        }
        for (const auto child : {left, right}) {
            if (child == lzss_hash_tree_null_node) continue;
            ++child_edge_count;
            if (!is_active_bucket_position(
                    context, child, context.nodes.position[child])
                || context.nodes.parent[child] != node) {
                return LzssHashTreeBucketBuildError::invalid_tree;
            }
        }
        if (left != lzss_hash_tree_null_node
            && compare_positions(
                   context, context.nodes.position[left], candidate) >= 0) {
            return LzssHashTreeBucketBuildError::invalid_tree;
        }
        if (right != lzss_hash_tree_null_node
            && compare_positions(
                   context, context.nodes.position[right], candidate) <= 0) {
            return LzssHashTreeBucketBuildError::invalid_tree;
        }
        const auto left_height = left == lzss_hash_tree_null_node
            ? std::uint8_t{0} : context.nodes.height[left];
        const auto right_height = right == lzss_hash_tree_null_node
            ? std::uint8_t{0} : context.nodes.height[right];
        const auto expected_height = static_cast<std::uint8_t>(
            static_cast<unsigned>(std::max(left_height, right_height)) + 1U);
        const auto balance = static_cast<int>(left_height)
            - static_cast<int>(right_height);
        auto expected_maximum = candidate;
        if (left != lzss_hash_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                static_cast<std::size_t>(
                    context.nodes.subtree_maximum_position[left]));
        }
        if (right != lzss_hash_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum,
                static_cast<std::size_t>(
                    context.nodes.subtree_maximum_position[right]));
        }
        if (context.nodes.height[node] != expected_height
            || balance < -1 || balance > 1
            || context.nodes.subtree_maximum_position[node]
                != expected_maximum) {
            return LzssHashTreeBucketBuildError::invalid_tree;
        }

        auto path = node;
        std::size_t path_length{};
        while (path != root) {
            if (++path_length >= expected_node_count) {
                return LzssHashTreeBucketBuildError::invalid_tree;
            }
            const auto parent = context.nodes.parent[path];
            if (parent >= capacity
                || !is_active_bucket_position(
                    context, parent, context.nodes.position[parent])
                || (context.nodes.left[parent] != path
                    && context.nodes.right[parent] != path)) {
                return LzssHashTreeBucketBuildError::invalid_tree;
            }
            path = parent;
        }

        const auto distance = context.links[candidate % capacity];
        if (distance == 0) break;
        candidate -= distance;
    }
    if (child_edge_count != expected_node_count - 1U) {
        return LzssHashTreeBucketBuildError::invalid_tree;
    }
    return LzssHashTreeBucketBuildError::none;
}

LzssHashTreeBucketBuildResult build_lzss_hash_tree_bucket(
    const LzssHashTreeBucketBuildContext& context) noexcept {
    LzssHashTreeBucketBuildResult result{};
    result.error = validate_context(context);
    if (result.error != LzssHashTreeBucketBuildError::none) return result;
    const auto chain = inspect_chain(context);
    if (chain.error != LzssHashTreeBucketBuildError::none) {
        result.error = chain.error;
        return result;
    }
    if (chain.node_count == 0) return result;

    BucketTreeBuilder builder{context};
    auto candidate = context.head_position;
    for (std::size_t index = 0; index < chain.node_count; ++index) {
        builder.insert(candidate);
        const auto distance = context.links[candidate % context.links.size()];
        if (distance == 0) break;
        candidate -= distance;
    }
    const auto root = builder.root();
    result.error = validate_lzss_hash_tree_bucket(
        context, root, chain.node_count);
    if (result.error != LzssHashTreeBucketBuildError::none) return result;
    result.root = root;
    result.node_count = chain.node_count;
    return result;
}

} // namespace marc::dictionary::internal

#include "dictionary/lzss_hash_tree_bucket_query.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>
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

[[nodiscard]] LzssHashTreeBucketQueryError validate_context(
    const LzssHashTreeBucketQueryContext& context) noexcept {
    const auto capacity = context.left.size();
    if (context.parameters.window_size == 0
        || context.parameters.max_match_length == 0) {
        return LzssHashTreeBucketQueryError::invalid_parameters;
    }
    if (context.bucket_count == 0
        || (context.bucket_count & (context.bucket_count - 1U)) != 0
        || context.bucket >= context.bucket_count) {
        return LzssHashTreeBucketQueryError::invalid_bucket;
    }
    if (!lzss_hash_tree_position_extent_representable(context.input.size())) {
        return LzssHashTreeBucketQueryError::invalid_parameters;
    }
    if (context.query_position > context.input.size()) {
        return LzssHashTreeBucketQueryError::invalid_query_position;
    }
    if (context.node_identity != LzssHashTreeNodeIdentity::ring_position
        && context.node_identity != LzssHashTreeNodeIdentity::pool_local) {
        return LzssHashTreeBucketQueryError::invalid_node_arrays;
    }
    const auto expected_ring_capacity = std::min<std::size_t>(
        context.input.size(), context.parameters.window_size);
    if (capacity == 0
        || capacity > static_cast<std::size_t>(UINT32_MAX)
        || (context.node_identity == LzssHashTreeNodeIdentity::ring_position
            && capacity != expected_ring_capacity)
        || context.right.size() != capacity
        || context.parent.size() != capacity
        || context.height.size() != capacity
        || context.position.size() != capacity
        || context.subtree_maximum_position.size() != capacity) {
        return LzssHashTreeBucketQueryError::invalid_node_arrays;
    }
    if (context.root != lzss_hash_tree_null_node
        && context.root >= capacity) {
        return LzssHashTreeBucketQueryError::invalid_root;
    }
    return LzssHashTreeBucketQueryError::none;
}

[[nodiscard]] bool validate_node(
    const LzssHashTreeBucketQueryContext& context,
    const std::uint32_t node) noexcept {
    if (node >= context.left.size()) return false;
    const auto position = context.position[node];
    if (position >= context.query_position
        || position >= context.input.size()
        || context.query_position - position
            > context.parameters.window_size
        || context.input.size() - position
            < lzss_match_finder_prefix_size
        || (context.node_identity == LzssHashTreeNodeIdentity::ring_position
            && position % context.left.size() != node)
        || (context.node_identity == LzssHashTreeNodeIdentity::pool_local
            && context.height[node] == 0)
        || (context.left[node] != lzss_hash_tree_null_node
            && context.left[node] >= context.left.size())
        || (context.right[node] != lzss_hash_tree_null_node
            && context.right[node] >= context.left.size())) {
        return false;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    return hash.valid
        && (static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) == context.bucket;
}

[[nodiscard]] int compare_positions(
    const LzssHashTreeBucketQueryContext& context,
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

[[nodiscard]] std::uint32_t common_prefix_length(
    const LzssHashTreeBucketQueryContext& context,
    const std::size_t left, const std::size_t right) noexcept {
    const auto maximum = std::min({
        context.input.size() - left,
        context.input.size() - right,
        static_cast<std::size_t>(context.parameters.max_match_length)});
    std::size_t length{};
    while (length < maximum) {
        if (context.statistics != nullptr) {
            increment_statistic(
                context.statistics,
                context.statistics->lcp_byte_comparison_count);
        }
        if (context.input[left + length] != context.input[right + length]) {
            break;
        }
        ++length;
    }
    return static_cast<std::uint32_t>(length);
}

[[nodiscard]] int compare_prefix(
    const LzssHashTreeBucketQueryContext& context,
    const std::size_t position, const std::uint32_t length) noexcept {
    if (context.statistics != nullptr) {
        increment_statistic(
            context.statistics,
            context.statistics->prefix_range_comparison_count);
    }
    for (std::size_t index = 0; index < length; ++index) {
        if (context.statistics != nullptr) {
            increment_statistic(
                context.statistics,
                context.statistics->prefix_range_byte_comparison_count);
        }
        const auto byte = std::to_integer<std::uint8_t>(
            context.input[position + index]);
        const auto query_byte = std::to_integer<std::uint8_t>(
            context.input[context.query_position + index]);
        if (byte < query_byte) return -1;
        if (byte > query_byte) return 1;
    }
    return 0;
}

[[nodiscard]] bool take_step(
    const std::size_t capacity, std::size_t& traversal_steps,
    std::uint64_t& total_nodes) noexcept {
    if (traversal_steps == capacity) return false;
    ++traversal_steps;
    if (total_nodes != std::numeric_limits<std::uint64_t>::max()) {
        ++total_nodes;
    }
    return true;
}

[[nodiscard]] std::uint32_t find_pool_local_node_by_position(
    const LzssHashTreeBucketQueryContext& context,
    const std::size_t position, std::uint64_t& total_nodes) noexcept {
    auto current = context.root;
    std::size_t traversal_steps{};
    while (current != lzss_hash_tree_null_node) {
        if (!take_step(
                context.left.size(), traversal_steps, total_nodes)
            || !validate_node(context, current)) {
            return lzss_hash_tree_null_node;
        }
        const auto comparison = compare_positions(
            context, position, context.position[current]);
        if (comparison == 0) return current;
        current = comparison < 0 ? context.left[current]
                                 : context.right[current];
    }
    return lzss_hash_tree_null_node;
}

[[nodiscard]] bool validate_snapshot_node(
    const LzssHashTreeBucketQueryContext& context,
    const std::uint32_t node) noexcept {
    if (node >= context.left.size()) return false;
    const auto position = context.position[node];
    if (position >= context.query_position
        || position >= context.input.size()
        || context.input.size() - position
            < lzss_match_finder_prefix_size
        || context.height[node] == 0) {
        return false;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    if (!hash.valid
        || (static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) != context.bucket) {
        return false;
    }

    const auto left = context.left[node];
    const auto right = context.right[node];
    const auto child_valid = [&](const std::uint32_t child) noexcept {
        return child == lzss_hash_tree_null_node
            || (child < context.left.size()
                && context.parent[child] == node
                && context.height[child] != 0
                && context.subtree_maximum_position[child]
                    < context.query_position);
    };
    if (!child_valid(left) || !child_valid(right)) return false;

    const auto child_height = [&](const std::uint32_t child) noexcept {
        return child == lzss_hash_tree_null_node
            ? std::uint8_t{0} : context.height[child];
    };
    const auto maximum_height = std::max(
        child_height(left), child_height(right));
    if (maximum_height == std::numeric_limits<std::uint8_t>::max()
        || context.height[node]
            != static_cast<std::uint8_t>(maximum_height + 1U)) {
        return false;
    }

    auto maximum_position = position;
    if (left != lzss_hash_tree_null_node) {
        maximum_position = std::max(
            maximum_position,
            context.subtree_maximum_position[left]);
    }
    if (right != lzss_hash_tree_null_node) {
        maximum_position = std::max(
            maximum_position,
            context.subtree_maximum_position[right]);
    }
    if (context.subtree_maximum_position[node] != maximum_position) {
        return false;
    }

    auto ordering_context = context;
    ordering_context.statistics = nullptr;
    return (left == lzss_hash_tree_null_node
            || compare_positions(
                   ordering_context, context.position[left], position) < 0)
        && (right == lzss_hash_tree_null_node
            || compare_positions(
                   ordering_context, position, context.position[right]) < 0);
}

} // namespace

LzssHashTreeBucketQueryResult query_lzss_hash_tree_bucket_exact(
    const LzssHashTreeBucketQueryContext& context) noexcept {
    LzssHashTreeBucketQueryResult result{};
    result.error = validate_context(context);
    if (result.error != LzssHashTreeBucketQueryError::none) return result;
    if (context.root == lzss_hash_tree_null_node
        || context.input.size() - context.query_position
            < lzss_match_finder_prefix_size) {
        return result;
    }
    if (!validate_node(context, context.root)
        || context.parent[context.root] != lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketQueryError::invalid_root;
        return result;
    }

    auto predecessor = lzss_hash_tree_null_node;
    auto successor = lzss_hash_tree_null_node;
    auto current = context.root;
    std::size_t traversal_steps{};
    while (current != lzss_hash_tree_null_node) {
        if (!take_step(
                context.left.size(), traversal_steps,
                result.nodes_visited)
            || !validate_node(context, current)) {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        const auto comparison = compare_positions(
            context, context.query_position, context.position[current]);
        if (comparison < 0) {
            successor = current;
            current = context.left[current];
        } else if (comparison > 0) {
            predecessor = current;
            current = context.right[current];
        } else {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
    }

    std::uint32_t predecessor_lcp{};
    std::uint32_t successor_lcp{};
    if (predecessor != lzss_hash_tree_null_node) {
        predecessor_lcp = common_prefix_length(
            context, context.query_position,
            context.position[predecessor]);
    }
    if (successor != lzss_hash_tree_null_node) {
        successor_lcp = common_prefix_length(
            context, context.query_position,
            context.position[successor]);
    }
    result.maximum_lcp = std::max(predecessor_lcp, successor_lcp);
    if (result.maximum_lcp < context.parameters.min_match_length) {
        return result;
    }

    auto split = lzss_hash_tree_null_node;
    current = context.root;
    traversal_steps = 0;
    while (current != lzss_hash_tree_null_node) {
        if (!take_step(
                context.left.size(), traversal_steps,
                result.nodes_visited)
            || !validate_node(context, current)) {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        const auto comparison = compare_prefix(
            context, context.position[current], result.maximum_lcp);
        if (comparison < 0) current = context.right[current];
        else if (comparison > 0) current = context.left[current];
        else {
            split = current;
            break;
        }
    }
    if (split == lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketQueryError::invalid_tree;
        return result;
    }

    auto maximum_position = static_cast<std::size_t>(
        context.position[split]);
    current = context.left[split];
    traversal_steps = 0;
    while (current != lzss_hash_tree_null_node) {
        if (!take_step(
                context.left.size(), traversal_steps,
                result.nodes_visited)
            || !validate_node(context, current)) {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        const auto comparison = compare_prefix(
            context, context.position[current], result.maximum_lcp);
        if (comparison < 0) current = context.right[current];
        else if (comparison > 0) current = context.left[current];
        else {
            maximum_position = std::max(
                maximum_position,
                static_cast<std::size_t>(context.position[current]));
            const auto right = context.right[current];
            if (right != lzss_hash_tree_null_node) {
                if (!validate_node(context, right)) {
                    result.error = LzssHashTreeBucketQueryError::invalid_tree;
                    return result;
                }
                maximum_position = std::max(
                    maximum_position,
                    static_cast<std::size_t>(
                        context.subtree_maximum_position[right]));
            }
            current = context.left[current];
        }
    }

    current = context.right[split];
    traversal_steps = 0;
    while (current != lzss_hash_tree_null_node) {
        if (!take_step(
                context.left.size(), traversal_steps,
                result.nodes_visited)
            || !validate_node(context, current)) {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        const auto comparison = compare_prefix(
            context, context.position[current], result.maximum_lcp);
        if (comparison < 0) current = context.right[current];
        else if (comparison > 0) current = context.left[current];
        else {
            maximum_position = std::max(
                maximum_position,
                static_cast<std::size_t>(context.position[current]));
            const auto left = context.left[current];
            if (left != lzss_hash_tree_null_node) {
                if (!validate_node(context, left)) {
                    result.error = LzssHashTreeBucketQueryError::invalid_tree;
                    return result;
                }
                maximum_position = std::max(
                    maximum_position,
                    static_cast<std::size_t>(
                        context.subtree_maximum_position[left]));
            }
            current = context.right[current];
        }
    }

    if (maximum_position >= context.query_position
        || maximum_position >= context.input.size()
        || context.query_position - maximum_position
            > context.parameters.window_size
        || context.input.size() - maximum_position
            < lzss_match_finder_prefix_size) {
        result.error = LzssHashTreeBucketQueryError::invalid_tree;
        return result;
    }
    const auto candidate_node =
        context.node_identity == LzssHashTreeNodeIdentity::ring_position
        ? static_cast<std::uint32_t>(
              maximum_position % context.left.size())
        : find_pool_local_node_by_position(
              context, maximum_position, result.nodes_visited);
    if (!validate_node(context, candidate_node)
        || context.position[candidate_node] != maximum_position
        || common_prefix_length(
            context, context.query_position, maximum_position)
            != result.maximum_lcp) {
        result.error = LzssHashTreeBucketQueryError::invalid_tree;
        return result;
    }
    const auto distance = context.query_position - maximum_position;
    if (distance > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssHashTreeBucketQueryError::invalid_tree;
        return result;
    }
    result.candidate_position = maximum_position;
    result.match = {
        static_cast<std::uint32_t>(distance), result.maximum_lcp};
    return result;
}

LzssHashTreeBucketQueryResult query_lzss_hash_tree_snapshot_exact(
    const LzssHashTreeBucketQueryContext& context) noexcept {
    LzssHashTreeBucketQueryResult result{};
    result.error = validate_context(context);
    if (result.error != LzssHashTreeBucketQueryError::none) return result;
    if (context.node_identity != LzssHashTreeNodeIdentity::pool_local) {
        result.error = LzssHashTreeBucketQueryError::invalid_node_arrays;
        return result;
    }
    if (context.root == lzss_hash_tree_null_node
        || context.input.size() - context.query_position
            < lzss_match_finder_prefix_size) {
        return result;
    }
    if (context.parent[context.root] != lzss_hash_tree_null_node
        || context.left.size()
            > std::numeric_limits<std::size_t>::max() / 3U) {
        result.error = LzssHashTreeBucketQueryError::invalid_root;
        return result;
    }

    const auto window_begin = context.query_position
            > context.parameters.window_size
        ? context.query_position - context.parameters.window_size : 0U;
    const auto maximum_steps = context.left.size() * 3U;
    auto previous = lzss_hash_tree_null_node;
    auto current = context.root;
    std::size_t traversal_steps{};

    while (current != lzss_hash_tree_null_node) {
        if (traversal_steps == maximum_steps
            || current >= context.left.size()) {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        ++traversal_steps;

        const auto parent = context.parent[current];
        std::uint32_t next{lzss_hash_tree_null_node};
        if (previous == parent) {
            if (!validate_snapshot_node(context, current)) {
                result.error = current == context.root
                    ? LzssHashTreeBucketQueryError::invalid_root
                    : LzssHashTreeBucketQueryError::invalid_tree;
                return result;
            }
            ++result.nodes_visited;

            if (context.subtree_maximum_position[current] < window_begin) {
                next = parent;
            } else {
                const auto candidate = static_cast<std::size_t>(
                    context.position[current]);
                if (candidate >= window_begin) {
                    const auto length = common_prefix_length(
                        context, context.query_position, candidate);
                    if (length >= context.parameters.min_match_length
                        && (length > result.maximum_lcp
                            || (length == result.maximum_lcp
                                && (result.candidate_position
                                        == lzss_hash_tree_no_position
                                    || candidate
                                        > result.candidate_position)))) {
                        result.maximum_lcp = length;
                        result.candidate_position = candidate;
                    }
                }
                next = context.left[current] != lzss_hash_tree_null_node
                    ? context.left[current]
                    : (context.right[current] != lzss_hash_tree_null_node
                        ? context.right[current] : parent);
            }
        } else if (previous == context.left[current]) {
            next = context.right[current] != lzss_hash_tree_null_node
                ? context.right[current] : parent;
        } else if (previous == context.right[current]) {
            next = parent;
        } else {
            result.error = LzssHashTreeBucketQueryError::invalid_tree;
            return result;
        }
        previous = current;
        current = next;
    }

    if (result.candidate_position == lzss_hash_tree_no_position) {
        return result;
    }
    const auto distance = context.query_position - result.candidate_position;
    if (distance > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssHashTreeBucketQueryError::invalid_tree;
        result.match = {};
        result.candidate_position = lzss_hash_tree_no_position;
        result.maximum_lcp = 0;
        return result;
    }
    result.match = {
        static_cast<std::uint32_t>(distance), result.maximum_lcp};
    return result;
}

} // namespace marc::dictionary::internal

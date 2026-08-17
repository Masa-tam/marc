#include "dictionary/lzss_hash_tree_bucket_mutation.hpp"

#include "dictionary/lzss_prefix_hash.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] LzssHashTreeBucketMutationError validate_context(
    const LzssHashTreeBucketMutationContext& context) noexcept {
    const auto capacity = context.left.size();
    if (context.parameters.window_size == 0
        || context.parameters.max_match_length == 0) {
        return LzssHashTreeBucketMutationError::invalid_parameters;
    }
    if (context.bucket_count == 0
        || !std::has_single_bit(context.bucket_count)
        || context.bucket >= context.bucket_count) {
        return LzssHashTreeBucketMutationError::invalid_bucket;
    }
    const auto expected_capacity = std::min<std::size_t>(
        context.input.size(), context.parameters.window_size);
    if (capacity == 0 || capacity != expected_capacity
        || context.right.size() != capacity
        || context.parent.size() != capacity
        || context.height.size() != capacity
        || context.position.size() != capacity
        || context.subtree_maximum_position.size() != capacity) {
        return LzssHashTreeBucketMutationError::invalid_node_arrays;
    }
    return LzssHashTreeBucketMutationError::none;
}

[[nodiscard]] bool position_belongs_to_bucket(
    const LzssHashTreeBucketMutationContext& context,
    const std::size_t position) noexcept {
    if (position >= context.input.size()
        || context.input.size() - position
            < lzss_match_finder_prefix_size) {
        return false;
    }
    const auto hash = calculate_lzss_prefix_hash(context.input, position);
    return hash.valid
        && (static_cast<std::size_t>(hash.value)
                & (context.bucket_count - 1U)) == context.bucket;
}

[[nodiscard]] bool valid_node(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t node) noexcept {
    if (node >= context.left.size()) return false;
    const auto position = context.position[node];
    return position_belongs_to_bucket(context, position)
        && position % context.left.size() == node
        && (context.left[node] == lzss_hash_tree_null_node
            || context.left[node] < context.left.size())
        && (context.right[node] == lzss_hash_tree_null_node
            || context.right[node] < context.left.size());
}

[[nodiscard]] int compare_positions(
    const LzssHashTreeBucketMutationContext& context,
    const std::size_t left, const std::size_t right) noexcept {
    const auto left_size = std::min<std::size_t>(
        context.input.size() - left, context.parameters.max_match_length);
    const auto right_size = std::min<std::size_t>(
        context.input.size() - right, context.parameters.max_match_length);
    const auto common_size = std::min(left_size, right_size);
    for (std::size_t index = 0; index < common_size; ++index) {
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

[[nodiscard]] bool valid_local_node(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t node) noexcept {
    if (!valid_node(context, node)) return false;
    const auto left = context.left[node];
    const auto right = context.right[node];
    for (const auto child : {left, right}) {
        if (child == lzss_hash_tree_null_node) continue;
        if (child == node || !valid_node(context, child)
            || context.parent[child] != node) {
            return false;
        }
    }
    if (left != lzss_hash_tree_null_node
        && compare_positions(
            context, context.position[left], context.position[node]) >= 0) {
        return false;
    }
    if (right != lzss_hash_tree_null_node
        && compare_positions(
            context, context.position[right], context.position[node]) <= 0) {
        return false;
    }
    const auto left_height = left == lzss_hash_tree_null_node
        ? std::uint8_t{0} : context.height[left];
    const auto right_height = right == lzss_hash_tree_null_node
        ? std::uint8_t{0} : context.height[right];
    const auto balance = static_cast<int>(left_height)
        - static_cast<int>(right_height);
    const auto expected_height = static_cast<std::uint8_t>(
        static_cast<unsigned>(std::max(left_height, right_height)) + 1U);
    auto expected_maximum = context.position[node];
    if (left != lzss_hash_tree_null_node) {
        expected_maximum = std::max(
            expected_maximum, context.subtree_maximum_position[left]);
    }
    if (right != lzss_hash_tree_null_node) {
        expected_maximum = std::max(
            expected_maximum, context.subtree_maximum_position[right]);
    }
    return context.height[node] == expected_height
        && balance >= -1 && balance <= 1
        && context.subtree_maximum_position[node] == expected_maximum;
}

struct SearchResult {
    std::uint32_t node{lzss_hash_tree_null_node};
    std::uint32_t parent{lzss_hash_tree_null_node};
    int order{};
    LzssHashTreeBucketMutationError error{
        LzssHashTreeBucketMutationError::none};
};

[[nodiscard]] SearchResult search_tree(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t root, const std::size_t position) noexcept {
    SearchResult result{};
    auto current = root;
    std::size_t visited{};
    while (current != lzss_hash_tree_null_node) {
        if (visited++ == context.left.size()
            || !valid_local_node(context, current)) {
            result.error = LzssHashTreeBucketMutationError::invalid_tree;
            return result;
        }
        result.parent = current;
        result.order = compare_positions(
            context, position, context.position[current]);
        if (result.order == 0) {
            result.node = current;
            return result;
        }
        current = result.order < 0 ? context.left[current]
                                   : context.right[current];
    }
    return result;
}

class MutableBucketTree {
public:
    MutableBucketTree(
        const LzssHashTreeBucketMutationContext& context,
        const std::uint32_t root) noexcept
        : context_(context), root_(root) {}

    [[nodiscard]] std::uint32_t root() const noexcept { return root_; }

    void insert(
        const std::size_t absolute_position,
        const std::uint32_t parent, const int order) noexcept {
        const auto node = static_cast<std::uint32_t>(
            absolute_position % context_.left.size());
        std::construct_at(context_.left.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.right.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.parent.data() + node,
                          lzss_hash_tree_null_node);
        std::construct_at(context_.height.data() + node, std::uint8_t{1});
        std::construct_at(context_.position.data() + node, absolute_position);
        std::construct_at(
            context_.subtree_maximum_position.data() + node,
            absolute_position);
        if (parent == lzss_hash_tree_null_node) {
            root_ = node;
            return;
        }
        context_.parent[node] = parent;
        if (order < 0) context_.left[parent] = node;
        else context_.right[parent] = node;
        rebalance_from(parent);
    }

    void remove(const std::uint32_t removed) noexcept {
        auto rebalance_start = lzss_hash_tree_null_node;
        if (context_.left[removed] == lzss_hash_tree_null_node) {
            rebalance_start = context_.parent[removed];
            replace_parent_child(
                context_.parent[removed], removed, context_.right[removed]);
        } else if (context_.right[removed] == lzss_hash_tree_null_node) {
            rebalance_start = context_.parent[removed];
            replace_parent_child(
                context_.parent[removed], removed, context_.left[removed]);
        } else {
            const auto successor = minimum(context_.right[removed]);
            if (context_.parent[successor] != removed) {
                rebalance_start = context_.parent[successor];
                replace_parent_child(
                    context_.parent[successor], successor,
                    context_.right[successor]);
                context_.right[successor] = context_.right[removed];
                context_.parent[context_.right[successor]] = successor;
            } else {
                rebalance_start = successor;
            }
            replace_parent_child(
                context_.parent[removed], removed, successor);
            context_.left[successor] = context_.left[removed];
            context_.parent[context_.left[successor]] = successor;
            update(successor);
        }
        clear(removed);
        rebalance_from(rebalance_start);
    }

private:
    [[nodiscard]] std::uint8_t node_height(
        const std::uint32_t node) const noexcept {
        return node == lzss_hash_tree_null_node ? std::uint8_t{0}
                                                : context_.height[node];
    }

    void update(const std::uint32_t node) noexcept {
        const auto left = context_.left[node];
        const auto right = context_.right[node];
        context_.height[node] = static_cast<std::uint8_t>(
            static_cast<unsigned>(
                std::max(node_height(left), node_height(right))) + 1U);
        auto maximum = context_.position[node];
        if (left != lzss_hash_tree_null_node) {
            maximum = std::max(
                maximum, context_.subtree_maximum_position[left]);
        }
        if (right != lzss_hash_tree_null_node) {
            maximum = std::max(
                maximum, context_.subtree_maximum_position[right]);
        }
        context_.subtree_maximum_position[node] = maximum;
    }

    void replace_parent_child(
        const std::uint32_t parent, const std::uint32_t previous,
        const std::uint32_t replacement) noexcept {
        if (parent == lzss_hash_tree_null_node) root_ = replacement;
        else if (context_.left[parent] == previous) {
            context_.left[parent] = replacement;
        } else {
            context_.right[parent] = replacement;
        }
        if (replacement != lzss_hash_tree_null_node) {
            context_.parent[replacement] = parent;
        }
    }

    [[nodiscard]] std::uint32_t rotate_left(
        const std::uint32_t node) noexcept {
        const auto promoted = context_.right[node];
        const auto transferred = context_.left[promoted];
        const auto parent = context_.parent[node];
        replace_parent_child(parent, node, promoted);
        context_.left[promoted] = node;
        context_.parent[node] = promoted;
        context_.right[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            context_.parent[transferred] = node;
        }
        update(node);
        update(promoted);
        return promoted;
    }

    [[nodiscard]] std::uint32_t rotate_right(
        const std::uint32_t node) noexcept {
        const auto promoted = context_.left[node];
        const auto transferred = context_.right[promoted];
        const auto parent = context_.parent[node];
        replace_parent_child(parent, node, promoted);
        context_.right[promoted] = node;
        context_.parent[node] = promoted;
        context_.left[node] = transferred;
        if (transferred != lzss_hash_tree_null_node) {
            context_.parent[transferred] = node;
        }
        update(node);
        update(promoted);
        return promoted;
    }

    [[nodiscard]] int balance_factor(
        const std::uint32_t node) const noexcept {
        return static_cast<int>(node_height(context_.left[node]))
            - static_cast<int>(node_height(context_.right[node]));
    }

    void rebalance_from(std::uint32_t node) noexcept {
        while (node != lzss_hash_tree_null_node) {
            update(node);
            auto subtree_root = node;
            const auto balance = balance_factor(node);
            if (balance == 2) {
                const auto left = context_.left[node];
                if (balance_factor(left) < 0) {
                    static_cast<void>(rotate_left(left));
                }
                subtree_root = rotate_right(node);
            } else if (balance == -2) {
                const auto right = context_.right[node];
                if (balance_factor(right) > 0) {
                    static_cast<void>(rotate_right(right));
                }
                subtree_root = rotate_left(node);
            }
            node = context_.parent[subtree_root];
        }
    }

    [[nodiscard]] std::uint32_t minimum(std::uint32_t node) const noexcept {
        while (context_.left[node] != lzss_hash_tree_null_node) {
            node = context_.left[node];
        }
        return node;
    }

    void clear(const std::uint32_t node) noexcept {
        context_.left[node] = lzss_hash_tree_null_node;
        context_.right[node] = lzss_hash_tree_null_node;
        context_.parent[node] = lzss_hash_tree_null_node;
        context_.height[node] = 0;
        context_.position[node] = lzss_hash_tree_no_position;
        context_.subtree_maximum_position[node] = lzss_hash_tree_no_position;
    }

    const LzssHashTreeBucketMutationContext& context_;
    std::uint32_t root_;
};

[[nodiscard]] bool active_position(
    const LzssHashTreeBucketMutationContext& context,
    const std::size_t position, const std::size_t begin,
    const std::size_t end) noexcept {
    return position >= begin && position < end
        && position_belongs_to_bucket(context, position);
}

} // namespace

LzssHashTreeBucketMutationResult insert_lzss_hash_tree_bucket_position(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t root, const std::size_t position) noexcept {
    LzssHashTreeBucketMutationResult result{root};
    result.error = validate_context(context);
    if (result.error != LzssHashTreeBucketMutationError::none) return result;
    if (!position_belongs_to_bucket(context, position)) {
        result.error = LzssHashTreeBucketMutationError::invalid_position;
        return result;
    }
    if (root != lzss_hash_tree_null_node
        && (root >= context.left.size() || !valid_node(context, root)
            || context.parent[root] != lzss_hash_tree_null_node)) {
        result.error = LzssHashTreeBucketMutationError::invalid_root;
        return result;
    }
    const auto search = search_tree(context, root, position);
    if (search.error != LzssHashTreeBucketMutationError::none) {
        result.error = search.error;
        return result;
    }
    if (search.node != lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketMutationError::duplicate_position;
        return result;
    }
    MutableBucketTree tree{context, root};
    tree.insert(position, search.parent, search.order);
    result.root = tree.root();
    return result;
}

LzssHashTreeBucketMutationResult remove_lzss_hash_tree_bucket_position(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t root, const std::size_t position) noexcept {
    LzssHashTreeBucketMutationResult result{root};
    result.error = validate_context(context);
    if (result.error != LzssHashTreeBucketMutationError::none) return result;
    if (!position_belongs_to_bucket(context, position)) {
        result.error = LzssHashTreeBucketMutationError::invalid_position;
        return result;
    }
    if (root == lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketMutationError::missing_position;
        return result;
    }
    if (root >= context.left.size() || !valid_node(context, root)
        || context.parent[root] != lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketMutationError::invalid_root;
        return result;
    }
    const auto search = search_tree(context, root, position);
    if (search.error != LzssHashTreeBucketMutationError::none) {
        result.error = search.error;
        return result;
    }
    if (search.node == lzss_hash_tree_null_node) {
        result.error = LzssHashTreeBucketMutationError::missing_position;
        return result;
    }
    if (context.left[search.node] != lzss_hash_tree_null_node
        && context.right[search.node] != lzss_hash_tree_null_node) {
        auto successor = context.right[search.node];
        std::size_t visited{};
        while (true) {
            if (visited++ == context.left.size()
                || !valid_local_node(context, successor)) {
                result.error = LzssHashTreeBucketMutationError::invalid_tree;
                return result;
            }
            if (context.left[successor] == lzss_hash_tree_null_node) break;
            successor = context.left[successor];
        }
    }
    MutableBucketTree tree{context, root};
    tree.remove(search.node);
    result.root = tree.root();
    return result;
}

LzssHashTreeBucketMutationError validate_lzss_hash_tree_bucket_active_range(
    const LzssHashTreeBucketMutationContext& context,
    const std::uint32_t root, const std::size_t active_begin,
    const std::size_t active_end) noexcept {
    const auto context_error = validate_context(context);
    if (context_error != LzssHashTreeBucketMutationError::none) {
        return context_error;
    }
    if (active_begin > active_end || active_end > context.input.size()
        || active_end - active_begin > context.parameters.window_size) {
        return LzssHashTreeBucketMutationError::invalid_position;
    }
    std::size_t expected_count{};
    for (auto position = active_begin; position < active_end; ++position) {
        if (position_belongs_to_bucket(context, position)) ++expected_count;
    }
    if (expected_count == 0) {
        return root == lzss_hash_tree_null_node
            ? LzssHashTreeBucketMutationError::none
            : LzssHashTreeBucketMutationError::invalid_tree;
    }
    if (root >= context.left.size() || !valid_node(context, root)
        || !active_position(
            context, context.position[root], active_begin, active_end)
        || context.parent[root] != lzss_hash_tree_null_node) {
        return LzssHashTreeBucketMutationError::invalid_root;
    }

    std::size_t edge_count{};
    std::size_t actual_count{};
    for (auto position = active_begin; position < active_end; ++position) {
        if (!position_belongs_to_bucket(context, position)) continue;
        ++actual_count;
        const auto node = static_cast<std::uint32_t>(
            position % context.left.size());
        if (!valid_node(context, node) || context.position[node] != position) {
            return LzssHashTreeBucketMutationError::invalid_tree;
        }
        const auto left = context.left[node];
        const auto right = context.right[node];
        for (const auto child : {left, right}) {
            if (child == lzss_hash_tree_null_node) continue;
            ++edge_count;
            if (!valid_node(context, child)
                || !active_position(
                    context, context.position[child], active_begin, active_end)
                || context.parent[child] != node) {
                return LzssHashTreeBucketMutationError::invalid_tree;
            }
        }
        if (left != lzss_hash_tree_null_node
            && compare_positions(
                context, context.position[left], position) >= 0) {
            return LzssHashTreeBucketMutationError::invalid_tree;
        }
        if (right != lzss_hash_tree_null_node
            && compare_positions(
                context, context.position[right], position) <= 0) {
            return LzssHashTreeBucketMutationError::invalid_tree;
        }
        const auto left_height = left == lzss_hash_tree_null_node
            ? std::uint8_t{0} : context.height[left];
        const auto right_height = right == lzss_hash_tree_null_node
            ? std::uint8_t{0} : context.height[right];
        const auto balance = static_cast<int>(left_height)
            - static_cast<int>(right_height);
        const auto expected_height = static_cast<std::uint8_t>(
            static_cast<unsigned>(std::max(left_height, right_height)) + 1U);
        auto expected_maximum = position;
        if (left != lzss_hash_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum, context.subtree_maximum_position[left]);
        }
        if (right != lzss_hash_tree_null_node) {
            expected_maximum = std::max(
                expected_maximum, context.subtree_maximum_position[right]);
        }
        if (context.height[node] != expected_height
            || balance < -1 || balance > 1
            || context.subtree_maximum_position[node] != expected_maximum) {
            return LzssHashTreeBucketMutationError::invalid_tree;
        }

        auto path = node;
        std::size_t path_length{};
        while (path != root) {
            if (++path_length >= expected_count) {
                return LzssHashTreeBucketMutationError::invalid_tree;
            }
            const auto parent = context.parent[path];
            if (parent >= context.left.size()
                || !valid_node(context, parent)
                || !active_position(
                    context, context.position[parent],
                    active_begin, active_end)
                || (context.left[parent] != path
                    && context.right[parent] != path)) {
                return LzssHashTreeBucketMutationError::invalid_tree;
            }
            path = parent;
        }
    }
    if (actual_count != expected_count || edge_count != expected_count - 1U) {
        return LzssHashTreeBucketMutationError::invalid_tree;
    }
    return LzssHashTreeBucketMutationError::none;
}

} // namespace marc::dictionary::internal

#include "dictionary/lzss_sparse_hash_tree_snapshot_lifecycle.hpp"

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool same_node_arrays(
    const LzssSparseHashTreeNodeArrays& nodes,
    const LzssHashTreeBucketQueryContext& snapshot) noexcept {
    return nodes.left.data() == snapshot.left.data()
        && nodes.left.size() == snapshot.left.size()
        && nodes.right.data() == snapshot.right.data()
        && nodes.right.size() == snapshot.right.size()
        && nodes.parent.data() == snapshot.parent.data()
        && nodes.parent.size() == snapshot.parent.size()
        && nodes.height.data() == snapshot.height.data()
        && nodes.height.size() == snapshot.height.size()
        && nodes.position.data() == snapshot.position.data()
        && nodes.position.size() == snapshot.position.size()
        && nodes.subtree_maximum_position.data()
            == snapshot.subtree_maximum_position.data()
        && nodes.subtree_maximum_position.size()
            == snapshot.subtree_maximum_position.size();
}

} // namespace

LzssSparseHashTreeSnapshotReleaseResult
release_lzss_sparse_hash_tree_snapshot(
    const LzssSparseHashTreeSnapshotReleaseContext& context) noexcept {
    LzssSparseHashTreeSnapshotReleaseResult result{};
    if (context.pool == nullptr || !context.pool->initialized()
        || !context.pool->state_valid()
        || context.expected_node_count == 0
        || context.snapshot.root == lzss_hash_tree_null_node
        || context.snapshot.node_identity
            != LzssHashTreeNodeIdentity::pool_local
        || !same_node_arrays(
            context.pool->node_arrays(), context.snapshot)) {
        result.error =
            LzssSparseHashTreeSnapshotReleaseError::invalid_context;
        return result;
    }

    const auto validation = validate_lzss_hash_tree_snapshot(
        context.snapshot, context.expected_node_count);
    result.validation_error = validation.error;
    if (validation.error != LzssHashTreeBucketQueryError::none) {
        result.error =
            LzssSparseHashTreeSnapshotReleaseError::validation_failure;
        return result;
    }

    auto nodes = context.pool->node_arrays();
    auto root = context.snapshot.root;
    while (root != lzss_hash_tree_null_node) {
        if (result.released_node_count == context.expected_node_count) {
            result.error = LzssSparseHashTreeSnapshotReleaseError::pool_failure;
            return result;
        }
        auto leaf = root;
        std::size_t descent{};
        while (nodes.left[leaf] != lzss_hash_tree_null_node
               || nodes.right[leaf] != lzss_hash_tree_null_node) {
            if (descent++ == context.expected_node_count) {
                result.error =
                    LzssSparseHashTreeSnapshotReleaseError::pool_failure;
                return result;
            }
            leaf = nodes.left[leaf] != lzss_hash_tree_null_node
                ? nodes.left[leaf] : nodes.right[leaf];
        }
        const auto parent = nodes.parent[leaf];
        if (parent == lzss_hash_tree_null_node) {
            root = lzss_hash_tree_null_node;
        } else if (nodes.left[parent] == leaf) {
            nodes.left[parent] = lzss_hash_tree_null_node;
        } else {
            nodes.right[parent] = lzss_hash_tree_null_node;
        }
        result.pool_error = context.pool->release(leaf);
        if (result.pool_error != LzssSparseHashTreeError::none) {
            result.error = LzssSparseHashTreeSnapshotReleaseError::pool_failure;
            return result;
        }
        ++result.released_node_count;
    }
    if (result.released_node_count != context.expected_node_count) {
        result.error = LzssSparseHashTreeSnapshotReleaseError::pool_failure;
    }
    return result;
}

} // namespace marc::dictionary::internal

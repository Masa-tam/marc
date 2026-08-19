#include "dictionary/lzss_sparse_hash_tree_bucket_state.hpp"

#include <limits>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] LzssSparseHashTreeBucketTransitionResult initial_result(
    const LzssSparseHashTreeBucketMode mode, const std::uint32_t root,
    const std::size_t node_count) noexcept {
    LzssSparseHashTreeBucketTransitionResult result{};
    result.mode = mode;
    result.root = root;
    result.node_count = node_count;
    return result;
}

[[nodiscard]] bool same_node_arrays(
    const LzssSparseHashTreeNodeArrays& pool_nodes,
    const LzssHashTreeBucketMutationContext& context) noexcept {
    return pool_nodes.left.data() == context.left.data()
        && pool_nodes.left.size() == context.left.size()
        && pool_nodes.right.data() == context.right.data()
        && pool_nodes.right.size() == context.right.size()
        && pool_nodes.parent.data() == context.parent.data()
        && pool_nodes.parent.size() == context.parent.size()
        && pool_nodes.height.data() == context.height.data()
        && pool_nodes.height.size() == context.height.size()
        && pool_nodes.position.data() == context.position.data()
        && pool_nodes.position.size() == context.position.size()
        && pool_nodes.subtree_maximum_position.data()
            == context.subtree_maximum_position.data()
        && pool_nodes.subtree_maximum_position.size()
            == context.subtree_maximum_position.size();
}

} // namespace

LzssSparseHashTreeBucketTransitionResult
promote_lzss_sparse_hash_tree_bucket(
    const LzssSparseHashTreeBucketBuildContext& context,
    const LzssSparseHashTreeBucketMode mode, const std::uint32_t root,
    const std::size_t node_count) noexcept {
    auto result = initial_result(mode, root, node_count);
    if (mode != LzssSparseHashTreeBucketMode::chain) {
        result.error = LzssSparseHashTreeBucketTransitionError::invalid_mode;
        return result;
    }
    if (root != lzss_hash_tree_null_node || node_count != 0) {
        result.error =
            LzssSparseHashTreeBucketTransitionError::invalid_metadata;
        return result;
    }

    const auto build = build_lzss_sparse_hash_tree_bucket(context);
    result.build_error = build.error;
    if (build.error != LzssSparseHashTreeBucketBuildError::none) {
        result.error =
            LzssSparseHashTreeBucketTransitionError::build_failure;
        return result;
    }
    if (build.status ==
        LzssSparseHashTreeBucketBuildStatus::insufficient_capacity) {
        result.mode = LzssSparseHashTreeBucketMode::pool_rejected_chain;
        result.status =
            LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain;
        return result;
    }
    if (build.status == LzssSparseHashTreeBucketBuildStatus::empty) {
        return result;
    }
    result.root = build.root;
    result.node_count = build.node_count;
    result.mode = LzssSparseHashTreeBucketMode::promoted_tree;
    result.status = LzssSparseHashTreeBucketTransitionStatus::promoted;
    return result;
}

LzssSparseHashTreeBucketTransitionResult
insert_lzss_sparse_hash_tree_bucket_or_demote(
    const LzssSparseHashTreeBucketBuildContext& release_context,
    const LzssHashTreeBucketMutationContext& mutation_context,
    const LzssSparseHashTreeBucketMode mode, const std::uint32_t root,
    const std::size_t node_count, const std::size_t position) noexcept {
    auto result = initial_result(mode, root, node_count);
    if (mode != LzssSparseHashTreeBucketMode::promoted_tree) {
        result.error = LzssSparseHashTreeBucketTransitionError::invalid_mode;
        return result;
    }
    if (root == lzss_hash_tree_null_node || node_count == 0
        || release_context.pool == nullptr
        || mutation_context.node_identity
            != LzssHashTreeNodeIdentity::pool_local
        || !same_node_arrays(
            release_context.pool->node_arrays(), mutation_context)) {
        result.error =
            LzssSparseHashTreeBucketTransitionError::invalid_metadata;
        return result;
    }
    if (node_count == std::numeric_limits<std::size_t>::max()) {
        result.error =
            LzssSparseHashTreeBucketTransitionError::arithmetic_overflow;
        return result;
    }

    if (release_context.pool->free_count() == 0) {
        result.build_error = release_lzss_sparse_hash_tree_bucket(
            release_context, root, node_count);
        if (result.build_error !=
            LzssSparseHashTreeBucketBuildError::none) {
            result.error =
                LzssSparseHashTreeBucketTransitionError::build_failure;
            return result;
        }
        result.root = lzss_hash_tree_null_node;
        result.node_count = 0;
        result.mode = LzssSparseHashTreeBucketMode::pool_rejected_chain;
        result.status =
            LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain;
        return result;
    }

    const auto allocation = release_context.pool->allocate();
    result.pool_error = allocation.error;
    if (!allocation.allocated
        || allocation.error != LzssSparseHashTreeError::none) {
        result.error =
            LzssSparseHashTreeBucketTransitionError::pool_failure;
        return result;
    }
    const auto mutation = insert_lzss_hash_tree_bucket_pool_node_v2(
        mutation_context, root, position, allocation.node);
    result.mutation_error = mutation.error;
    if (mutation.error != LzssHashTreeBucketMutationError::none) {
        result.pool_error = release_context.pool->release(allocation.node);
        result.error = result.pool_error == LzssSparseHashTreeError::none
            ? LzssSparseHashTreeBucketTransitionError::mutation_failure
            : LzssSparseHashTreeBucketTransitionError::pool_failure;
        return result;
    }
    result.root = mutation.root;
    result.node_count = node_count + 1U;
    result.status = LzssSparseHashTreeBucketTransitionStatus::inserted;
    return result;
}

} // namespace marc::dictionary::internal

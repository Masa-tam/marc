#include "dictionary/lzss_sparse_hash_tree_bucket_state.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result{};
    for (const auto value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

struct AlignedStorage {
    std::vector<std::max_align_t> words{};
    std::span<std::byte> bytes{};
};

[[nodiscard]] AlignedStorage make_storage(const std::size_t byte_count) {
    AlignedStorage result{};
    result.words.resize((byte_count + sizeof(std::max_align_t) - 1U)
                        / sizeof(std::max_align_t));
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

struct StateFixture {
    explicit StateFixture(const std::size_t capacity)
        : input(bytes("AAAAAAAAAAAAAAAAAAAA")) {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        links.assign(input.size(), 0);
        links[10] = 5;
        links[5] = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, capacity);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                      input.size(), parameters, {}, capacity,
                      storage.bytes.first(required.workspace_size), pool),
                  LzssSparseHashTreeError::none);
    }

    [[nodiscard]] LzssSparseHashTreeBucketBuildContext build_context() {
        return {input, parameters, 15, 0, 1, 10, links, &pool, nullptr};
    }

    [[nodiscard]] LzssHashTreeBucketMutationContext mutation_context() {
        const auto nodes = pool.node_arrays();
        return {
            input, parameters, 0, 1,
            nodes.left, nodes.right, nodes.parent, nodes.height,
            nodes.position, nodes.subtree_maximum_position, nullptr,
            LzssHashTreeNodeIdentity::pool_local};
    }

    std::vector<std::byte> input{};
    LzssParameters parameters{};
    std::vector<std::uint32_t> links{};
    AlignedStorage storage{};
    LzssSparseHashTreeNodePool pool{};
};

TEST(LzssSparseHashTreeBucketState,
     PromotionCapacityRejectionPublishesTerminalChain) {
    StateFixture fixture{2};
    const auto transition = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(transition.status,
              LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain);
    EXPECT_EQ(transition.mode,
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(transition.root, lzss_hash_tree_null_node);
    EXPECT_EQ(transition.node_count, 0U);
    EXPECT_EQ(fixture.pool.free_count(), 2U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
}

TEST(LzssSparseHashTreeBucketState, PromotionPublishesCompleteTree) {
    StateFixture fixture{4};
    const auto transition = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(transition.status,
              LzssSparseHashTreeBucketTransitionStatus::promoted);
    EXPECT_EQ(transition.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_NE(transition.root, lzss_hash_tree_null_node);
    EXPECT_EQ(transition.node_count, 3U);
    EXPECT_EQ(fixture.pool.active_count(), 3U);
}

TEST(LzssSparseHashTreeBucketState,
     ExhaustedInsertionReleasesWholeTreeAndPublishesTerminalChain) {
    StateFixture fixture{3};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    ASSERT_EQ(fixture.pool.free_count(), 0U);

    const auto transition = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 11);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(transition.status,
              LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain);
    EXPECT_EQ(transition.mode,
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(transition.root, lzss_hash_tree_null_node);
    EXPECT_EQ(transition.node_count, 0U);
    EXPECT_EQ(fixture.pool.free_count(), 3U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
}

TEST(LzssSparseHashTreeBucketState, SpareCapacityInsertsPoolNode) {
    StateFixture fixture{4};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    const auto transition = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 11);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(transition.status,
              LzssSparseHashTreeBucketTransitionStatus::inserted);
    EXPECT_EQ(transition.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(transition.node_count, 4U);
    EXPECT_EQ(fixture.pool.free_count(), 0U);
    EXPECT_EQ(fixture.pool.active_count(), 4U);
}

TEST(LzssSparseHashTreeBucketState,
     TerminalChainCannotPromoteAgainWithinFrame) {
    StateFixture fixture{2};
    const auto rejected = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(rejected.mode,
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    const auto retry = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), rejected.mode, rejected.root,
        rejected.node_count);
    EXPECT_EQ(retry.error,
              LzssSparseHashTreeBucketTransitionError::invalid_mode);
    EXPECT_EQ(retry.mode,
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
}

TEST(LzssSparseHashTreeBucketState,
     MalformedChainIsHardFailureNotCapacityFallback) {
    StateFixture fixture{2};
    fixture.links[10] = 11;
    const auto transition = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::build_failure);
    EXPECT_EQ(transition.build_error,
              LzssSparseHashTreeBucketBuildError::invalid_link);
    EXPECT_EQ(transition.mode, LzssSparseHashTreeBucketMode::chain);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
}

TEST(LzssSparseHashTreeBucketState,
     MutationFailurePreservesPromotedMetadataAndAccounting) {
    StateFixture fixture{4};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    const auto transition = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 10);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::mutation_failure);
    EXPECT_EQ(transition.mutation_error,
              LzssHashTreeBucketMutationError::duplicate_position);
    EXPECT_EQ(transition.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(transition.root, promoted.root);
    EXPECT_EQ(transition.node_count, promoted.node_count);
    EXPECT_EQ(fixture.pool.free_count(), 1U);
    EXPECT_EQ(fixture.pool.active_count(), 3U);
}

TEST(LzssSparseHashTreeBucketState,
     CorruptPromotedTreeIsHardFailureWithoutPartialRelease) {
    StateFixture fixture{3};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    auto nodes = fixture.pool.node_arrays();
    nodes.subtree_maximum_position[promoted.root] =
        lzss_hash_tree_no_stored_position;

    const auto transition = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 11);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::build_failure);
    EXPECT_EQ(transition.build_error,
              LzssSparseHashTreeBucketBuildError::invalid_tree);
    EXPECT_EQ(transition.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(transition.root, promoted.root);
    EXPECT_EQ(transition.node_count, promoted.node_count);
    EXPECT_EQ(fixture.pool.free_count(), 0U);
    EXPECT_EQ(fixture.pool.active_count(), 3U);
    EXPECT_EQ(nodes.subtree_maximum_position[promoted.root],
              lzss_hash_tree_no_stored_position);
}

TEST(LzssSparseHashTreeBucketState,
     RetiresPromotedPositionAndReleasesDetachedNode) {
    StateFixture fixture{4};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
        fixture.pool, fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 5);
    EXPECT_EQ(retired.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(retired.status,
              LzssSparseHashTreeBucketTransitionStatus::retired);
    EXPECT_EQ(retired.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(retired.node_count, 2U);
    EXPECT_EQ(fixture.pool.free_count(), 2U);
    EXPECT_EQ(fixture.pool.active_count(), 2U);
}

TEST(LzssSparseHashTreeBucketState,
     SingleNodeRetirementLeavesReusableEmptyPromotedTree) {
    StateFixture fixture{1};
    fixture.links[10] = 0;
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.node_count, 1U);
    const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
        fixture.pool, fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 10);
    ASSERT_EQ(retired.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(retired.root, lzss_hash_tree_null_node);
    EXPECT_EQ(retired.node_count, 0U);
    EXPECT_EQ(retired.mode, LzssSparseHashTreeBucketMode::promoted_tree);
    ASSERT_EQ(fixture.pool.free_count(), 1U);

    const auto inserted = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(), retired.mode,
        retired.root, retired.node_count, 11);
    EXPECT_EQ(inserted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(inserted.status,
              LzssSparseHashTreeBucketTransitionStatus::inserted);
    EXPECT_NE(inserted.root, lzss_hash_tree_null_node);
    EXPECT_EQ(inserted.node_count, 1U);
}

TEST(LzssSparseHashTreeBucketState,
     EmptyPromotedTreeDemotesWhenOtherBucketsExhaustPool) {
    StateFixture fixture{1};
    const auto unrelated = fixture.pool.allocate();
    ASSERT_TRUE(unrelated.allocated);
    const auto transition = insert_lzss_sparse_hash_tree_bucket_or_demote(
        fixture.build_context(), fixture.mutation_context(),
        LzssSparseHashTreeBucketMode::promoted_tree,
        lzss_hash_tree_null_node, 0, 11);
    EXPECT_EQ(transition.error,
              LzssSparseHashTreeBucketTransitionError::none);
    EXPECT_EQ(transition.status,
              LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain);
    EXPECT_EQ(transition.mode,
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(fixture.pool.active_count(), 1U);
}

TEST(LzssSparseHashTreeBucketState,
     ChainModesIgnoreRetirementWithoutTouchingPool) {
    for (const auto mode : {LzssSparseHashTreeBucketMode::chain,
                            LzssSparseHashTreeBucketMode::pool_rejected_chain}) {
        StateFixture fixture{1};
        const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
            fixture.pool, fixture.mutation_context(), mode,
            lzss_hash_tree_null_node, 0, 5);
        EXPECT_EQ(retired.error,
                  LzssSparseHashTreeBucketTransitionError::none);
        EXPECT_EQ(retired.status,
                  LzssSparseHashTreeBucketTransitionStatus::unchanged);
        EXPECT_EQ(retired.mode, mode);
        EXPECT_EQ(fixture.pool.free_count(), 1U);
    }
}

TEST(LzssSparseHashTreeBucketState,
     MissingRetirementPositionPreservesTreeAndAccounting) {
    StateFixture fixture{4};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
        fixture.pool, fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 11);
    EXPECT_EQ(retired.error,
              LzssSparseHashTreeBucketTransitionError::mutation_failure);
    EXPECT_EQ(retired.mutation_error,
              LzssHashTreeBucketMutationError::missing_position);
    EXPECT_EQ(retired.root, promoted.root);
    EXPECT_EQ(retired.node_count, promoted.node_count);
    EXPECT_EQ(fixture.pool.free_count(), 1U);
    EXPECT_EQ(fixture.pool.active_count(), 3U);
}

TEST(LzssSparseHashTreeBucketState,
     StickyPoolFailureRejectsRetirementBeforeTreeMutation) {
    StateFixture fixture{4};
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(fixture.pool.release(4),
              LzssSparseHashTreeError::invalid_node);
    const auto retired = retire_lzss_sparse_hash_tree_bucket_position(
        fixture.pool, fixture.mutation_context(), promoted.mode,
        promoted.root, promoted.node_count, 5);
    EXPECT_EQ(retired.error,
              LzssSparseHashTreeBucketTransitionError::invalid_metadata);
    EXPECT_EQ(retired.root, promoted.root);
    EXPECT_EQ(retired.node_count, promoted.node_count);
}

} // namespace

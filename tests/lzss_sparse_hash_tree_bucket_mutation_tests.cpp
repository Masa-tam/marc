#include "dictionary/lzss_hash_tree_bucket_mutation.hpp"

#include "dictionary/lzss_hash_tree_bucket_query.hpp"
#include "dictionary/lzss_sparse_hash_tree_bucket_builder.hpp"

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
    result.reserve(text.size());
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
    const auto count = byte_count == 0 ? 0
        : (byte_count + sizeof(std::max_align_t) - 1U)
            / sizeof(std::max_align_t);
    result.words.resize(count);
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

struct NodeSnapshot {
    std::vector<std::uint32_t> left{};
    std::vector<std::uint32_t> right{};
    std::vector<std::uint32_t> parent{};
    std::vector<std::uint8_t> height{};
    std::vector<LzssHashTreeStoredPosition> position{};
    std::vector<LzssHashTreeStoredPosition> subtree_maximum{};

    [[nodiscard]] bool operator==(const NodeSnapshot&) const = default;
};

[[nodiscard]] NodeSnapshot snapshot(LzssSparseHashTreeNodePool& pool) {
    const auto nodes = pool.node_arrays();
    return {
        {nodes.left.begin(), nodes.left.end()},
        {nodes.right.begin(), nodes.right.end()},
        {nodes.parent.begin(), nodes.parent.end()},
        {nodes.height.begin(), nodes.height.end()},
        {nodes.position.begin(), nodes.position.end()},
        {nodes.subtree_maximum_position.begin(),
         nodes.subtree_maximum_position.end()},
    };
}

struct SparseMutationFixture {
    SparseMutationFixture() : input(bytes("AAAAAAAAAAAAAAAAAAAA")) {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        links.assign(input.size(), 0);
        links[10] = 5;
        links[5] = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, 4);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                      input.size(), parameters, {}, 4,
                      storage.bytes.first(required.workspace_size), pool),
                  LzssSparseHashTreeError::none);
        const auto build = build_lzss_sparse_hash_tree_bucket(
            build_context(10));
        EXPECT_EQ(build.error, LzssSparseHashTreeBucketBuildError::none);
        EXPECT_EQ(build.status, LzssSparseHashTreeBucketBuildStatus::built);
        root = build.root;
        node_count = build.node_count;
    }

    [[nodiscard]] LzssSparseHashTreeBucketBuildContext build_context(
        const std::size_t head) {
        return {input, parameters, 15, 0, 1, head, links, &pool, nullptr};
    }

    [[nodiscard]] LzssHashTreeBucketMutationContext mutation_context() {
        const auto nodes = pool.node_arrays();
        return {
            input, parameters, 0, 1,
            nodes.left, nodes.right, nodes.parent, nodes.height,
            nodes.position, nodes.subtree_maximum_position, nullptr,
            LzssHashTreeNodeIdentity::pool_local};
    }

    [[nodiscard]] LzssHashTreeBucketQueryContext query_context() {
        const auto nodes = pool.node_arrays();
        return {
            input, parameters, 15, 0, 1, root,
            nodes.left, nodes.right, nodes.parent, nodes.height,
            nodes.position, nodes.subtree_maximum_position, nullptr,
            LzssHashTreeNodeIdentity::pool_local};
    }

    std::vector<std::byte> input{};
    LzssParameters parameters{};
    std::vector<std::uint32_t> links{};
    AlignedStorage storage{};
    LzssSparseHashTreeNodePool pool{};
    std::uint32_t root{lzss_hash_tree_null_node};
    std::size_t node_count{};
};

TEST(LzssSparseHashTreeBucketMutation,
     InsertsReservedNodeAndDetachesAbsolutePosition) {
    SparseMutationFixture fixture{};
    const auto allocation = fixture.pool.allocate();
    ASSERT_TRUE(allocation.allocated);
    auto mutation = insert_lzss_hash_tree_bucket_pool_node_v2(
        fixture.mutation_context(), fixture.root, 11, allocation.node);
    ASSERT_EQ(mutation.error, LzssHashTreeBucketMutationError::none);
    EXPECT_EQ(mutation.affected_node, allocation.node);
    fixture.root = mutation.root;
    ++fixture.node_count;
    fixture.links[11] = 1;
    EXPECT_EQ(validate_lzss_sparse_hash_tree_bucket(
                  fixture.build_context(11), fixture.root,
                  fixture.node_count),
              LzssSparseHashTreeBucketBuildError::none);

    mutation = detach_lzss_hash_tree_bucket_pool_position_v2(
        fixture.mutation_context(), fixture.root, 5);
    ASSERT_EQ(mutation.error, LzssHashTreeBucketMutationError::none);
    ASSERT_NE(mutation.affected_node, lzss_hash_tree_null_node);
    fixture.root = mutation.root;
    --fixture.node_count;
    auto nodes = fixture.pool.node_arrays();
    EXPECT_EQ(nodes.height[mutation.affected_node], 1U);
    EXPECT_EQ(nodes.position[mutation.affected_node],
              lzss_hash_tree_no_stored_position);
    EXPECT_EQ(fixture.pool.release(mutation.affected_node),
              LzssSparseHashTreeError::none);
    fixture.links[10] = 10;
    EXPECT_EQ(validate_lzss_sparse_hash_tree_bucket(
                  fixture.build_context(11), fixture.root,
                  fixture.node_count),
              LzssSparseHashTreeBucketBuildError::none);

    const auto query = query_lzss_hash_tree_bucket_exact(
        fixture.query_context());
    ASSERT_EQ(query.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(query.candidate_position, 11U);
    EXPECT_EQ(query.match, (LzssMatch{4, 5}));
}

TEST(LzssSparseHashTreeBucketMutation,
     RejectsInvalidReservedNodeWithoutTreeMutation) {
    SparseMutationFixture fixture{};
    const auto allocation = fixture.pool.allocate();
    ASSERT_TRUE(allocation.allocated);
    auto nodes = fixture.pool.node_arrays();
    nodes.position[allocation.node] = 7;
    const auto before = snapshot(fixture.pool);
    const auto mutation = insert_lzss_hash_tree_bucket_pool_node_v2(
        fixture.mutation_context(), fixture.root, 11, allocation.node);
    EXPECT_EQ(mutation.error, LzssHashTreeBucketMutationError::invalid_node);
    EXPECT_EQ(mutation.root, fixture.root);
    EXPECT_EQ(mutation.affected_node, lzss_hash_tree_null_node);
    EXPECT_EQ(snapshot(fixture.pool), before);
    EXPECT_EQ(fixture.pool.release(allocation.node),
              LzssSparseHashTreeError::none);
}

TEST(LzssSparseHashTreeBucketMutation,
     DuplicateInsertPreservesReservedNodeAndTree) {
    SparseMutationFixture fixture{};
    const auto allocation = fixture.pool.allocate();
    ASSERT_TRUE(allocation.allocated);
    const auto before = snapshot(fixture.pool);
    const auto mutation = insert_lzss_hash_tree_bucket_pool_node_v2(
        fixture.mutation_context(), fixture.root, 10, allocation.node);
    EXPECT_EQ(mutation.error,
              LzssHashTreeBucketMutationError::duplicate_position);
    EXPECT_EQ(mutation.root, fixture.root);
    EXPECT_EQ(mutation.affected_node, lzss_hash_tree_null_node);
    EXPECT_EQ(snapshot(fixture.pool), before);
    EXPECT_EQ(fixture.pool.release(allocation.node),
              LzssSparseHashTreeError::none);
}

TEST(LzssSparseHashTreeBucketMutation,
     RingIdentityCannotUsePoolNodePrimitive) {
    SparseMutationFixture fixture{};
    const auto allocation = fixture.pool.allocate();
    ASSERT_TRUE(allocation.allocated);
    auto context = fixture.mutation_context();
    context.node_identity = LzssHashTreeNodeIdentity::ring_position;
    const auto mutation = insert_lzss_hash_tree_bucket_pool_node_v2(
        context, fixture.root, 11, allocation.node);
    EXPECT_EQ(mutation.error,
              LzssHashTreeBucketMutationError::invalid_node_arrays);
    EXPECT_EQ(fixture.pool.release(allocation.node),
              LzssSparseHashTreeError::none);
}

TEST(LzssSparseHashTreeBucketMutation,
     RejectsWrongPrimitiveAndCorruptTreeAtomically) {
    SparseMutationFixture fixture{};
    auto context = fixture.mutation_context();
    auto mutation = insert_lzss_hash_tree_bucket_position_v2(
        context, fixture.root, 11);
    EXPECT_EQ(mutation.error, LzssHashTreeBucketMutationError::invalid_node);

    auto nodes = fixture.pool.node_arrays();
    nodes.left[fixture.root] = fixture.root;
    const auto before = snapshot(fixture.pool);
    mutation = detach_lzss_hash_tree_bucket_pool_position_v2(
        context, fixture.root, 5);
    EXPECT_EQ(mutation.error, LzssHashTreeBucketMutationError::invalid_tree);
    EXPECT_EQ(mutation.root, fixture.root);
    EXPECT_EQ(mutation.affected_node, lzss_hash_tree_null_node);
    EXPECT_EQ(snapshot(fixture.pool), before);
}

} // namespace

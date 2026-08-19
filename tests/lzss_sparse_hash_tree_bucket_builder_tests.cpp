#include "dictionary/lzss_sparse_hash_tree_bucket_builder.hpp"

#include "dictionary/lzss_hash_tree_bucket_query.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
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

struct SparseBuildFixture {
    explicit SparseBuildFixture(
        const std::size_t pool_capacity,
        const std::string_view text = "AAAAAAAAAAAAAAAAAAAA")
        : input(bytes(text)) {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        links.assign(input.size(), 0);
        links[10] = 5;
        links[5] = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, pool_capacity);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                      input.size(), parameters, {}, pool_capacity,
                      storage.bytes.first(required.workspace_size), pool),
                  LzssSparseHashTreeError::none);
    }

    [[nodiscard]] LzssSparseHashTreeBucketBuildContext context() {
        return {input, parameters, 15, 0, 1, 10, links, &pool, nullptr};
    }

    [[nodiscard]] LzssHashTreeBucketQueryContext query_context(
        const std::uint32_t root) {
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
};

TEST(LzssSparseHashTreeBucketBuilder, BuildsValidatedExactPoolLocalTree) {
    SparseBuildFixture fixture{3};
    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    ASSERT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    EXPECT_EQ(result.status, LzssSparseHashTreeBucketBuildStatus::built);
    EXPECT_EQ(result.node_count, 3U);
    EXPECT_NE(result.root, lzss_hash_tree_null_node);
    EXPECT_EQ(fixture.pool.free_count(), 0U);
    EXPECT_EQ(fixture.pool.active_count(), 3U);
    EXPECT_EQ(validate_lzss_sparse_hash_tree_bucket(
                  fixture.context(), result.root, result.node_count),
              LzssSparseHashTreeBucketBuildError::none);

    const auto query = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(result.root));
    ASSERT_EQ(query.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(query.candidate_position, 10U);
    EXPECT_EQ(query.match, (LzssMatch{5, 5}));
}

TEST(LzssSparseHashTreeBucketBuilder, BuildsSingleAndDoubleRotationShapes) {
    for (const auto& [text, expected_root_position] :
         std::vector<std::pair<std::string_view, std::size_t>>{
             {"AAAAABBBBBCCCCCQQQQQ", 5},
             {"CCCCCBBBBBAAAAAQQQQQ", 5},
             {"BBBBBAAAAACCCCCQQQQQ", 0},
             {"BBBBBCCCCCAAAAAQQQQQ", 0}}) {
        SparseBuildFixture fixture{3, text};
        const auto result = build_lzss_sparse_hash_tree_bucket(
            fixture.context());
        ASSERT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
        ASSERT_EQ(result.status, LzssSparseHashTreeBucketBuildStatus::built);
        ASSERT_NE(result.root, lzss_hash_tree_null_node);
        const auto nodes = fixture.pool.node_arrays();
        EXPECT_EQ(nodes.position[result.root], expected_root_position);
        EXPECT_EQ(nodes.height[result.root], 2U);
        EXPECT_EQ(validate_lzss_sparse_hash_tree_bucket(
                      fixture.context(), result.root, result.node_count),
                  LzssSparseHashTreeBucketBuildError::none);
    }
}

TEST(LzssSparseHashTreeBucketBuilder, CapacityRejectionIsAtomic) {
    SparseBuildFixture fixture{2};
    const auto before = snapshot(fixture.pool);
    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    EXPECT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    EXPECT_EQ(result.status,
              LzssSparseHashTreeBucketBuildStatus::insufficient_capacity);
    EXPECT_EQ(result.root, lzss_hash_tree_null_node);
    EXPECT_EQ(result.node_count, 0U);
    EXPECT_EQ(fixture.pool.free_count(), 2U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
    EXPECT_EQ(snapshot(fixture.pool), before);
}

TEST(LzssSparseHashTreeBucketBuilder, EmptyChainAcceptsZeroCapacity) {
    SparseBuildFixture fixture{0};
    auto context = fixture.context();
    context.head_position = lzss_hash_tree_no_position;
    const auto result = build_lzss_sparse_hash_tree_bucket(context);
    EXPECT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    EXPECT_EQ(result.status, LzssSparseHashTreeBucketBuildStatus::empty);
    EXPECT_EQ(result.root, lzss_hash_tree_null_node);
    EXPECT_EQ(result.node_count, 0U);
    EXPECT_EQ(fixture.pool.free_count(), 0U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
}

TEST(LzssSparseHashTreeBucketBuilder,
     PreservesUnrelatedAllocationAndUsesNoncontiguousIds) {
    SparseBuildFixture fixture{5};
    const auto first = fixture.pool.allocate();
    const auto second = fixture.pool.allocate();
    const auto unrelated = fixture.pool.allocate();
    ASSERT_TRUE(first.allocated);
    ASSERT_TRUE(second.allocated);
    ASSERT_TRUE(unrelated.allocated);
    ASSERT_EQ(fixture.pool.release(second.node),
              LzssSparseHashTreeError::none);
    ASSERT_EQ(fixture.pool.release(first.node),
              LzssSparseHashTreeError::none);
    const auto before = snapshot(fixture.pool);

    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    ASSERT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    ASSERT_EQ(result.status, LzssSparseHashTreeBucketBuildStatus::built);
    EXPECT_EQ(result.node_count, 3U);
    EXPECT_EQ(fixture.pool.active_count(), 4U);
    const auto after = snapshot(fixture.pool);
    EXPECT_EQ(after.left[unrelated.node], before.left[unrelated.node]);
    EXPECT_EQ(after.right[unrelated.node], before.right[unrelated.node]);
    EXPECT_EQ(after.parent[unrelated.node], before.parent[unrelated.node]);
    EXPECT_EQ(after.height[unrelated.node], before.height[unrelated.node]);
    EXPECT_EQ(after.position[unrelated.node], before.position[unrelated.node]);
    EXPECT_EQ(after.subtree_maximum[unrelated.node],
              before.subtree_maximum[unrelated.node]);

    const auto query = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(result.root));
    ASSERT_EQ(query.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(query.match, (LzssMatch{5, 5}));
    ASSERT_EQ(release_lzss_sparse_hash_tree_bucket(
                  fixture.context(), result.root, result.node_count),
              LzssSparseHashTreeBucketBuildError::none);
    EXPECT_EQ(fixture.pool.active_count(), 1U);
    EXPECT_EQ(fixture.pool.free_count(), 4U);
    EXPECT_EQ(fixture.pool.release(unrelated.node),
              LzssSparseHashTreeError::none);
}

TEST(LzssSparseHashTreeBucketBuilder, MalformedChainDoesNotTouchPool) {
    SparseBuildFixture fixture{3};
    fixture.links[10] = 11;
    const auto before = snapshot(fixture.pool);
    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    EXPECT_EQ(result.error, LzssSparseHashTreeBucketBuildError::invalid_link);
    EXPECT_EQ(result.root, lzss_hash_tree_null_node);
    EXPECT_EQ(result.node_count, 0U);
    EXPECT_EQ(fixture.pool.free_count(), 3U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
    EXPECT_EQ(snapshot(fixture.pool), before);
}

TEST(LzssSparseHashTreeBucketBuilder, ReleaseRestoresAllPoolNodes) {
    SparseBuildFixture fixture{3};
    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    ASSERT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    ASSERT_EQ(result.status, LzssSparseHashTreeBucketBuildStatus::built);
    EXPECT_EQ(release_lzss_sparse_hash_tree_bucket(
                  fixture.context(), result.root, result.node_count),
              LzssSparseHashTreeBucketBuildError::none);
    EXPECT_EQ(fixture.pool.free_count(), 3U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
    EXPECT_TRUE(fixture.pool.state_valid());

    for (std::size_t index = 0; index < 3; ++index) {
        const auto allocation = fixture.pool.allocate();
        EXPECT_TRUE(allocation.allocated);
    }
    EXPECT_FALSE(fixture.pool.allocate().allocated);
}

TEST(LzssSparseHashTreeBucketBuilder, ValidatorRejectsCorruptMetadata) {
    SparseBuildFixture fixture{3};
    const auto result = build_lzss_sparse_hash_tree_bucket(fixture.context());
    ASSERT_EQ(result.error, LzssSparseHashTreeBucketBuildError::none);
    auto nodes = fixture.pool.node_arrays();
    nodes.subtree_maximum_position[result.root] =
        lzss_hash_tree_no_stored_position;
    const auto before_release = snapshot(fixture.pool);
    EXPECT_EQ(validate_lzss_sparse_hash_tree_bucket(
                  fixture.context(), result.root, result.node_count),
              LzssSparseHashTreeBucketBuildError::invalid_tree);
    EXPECT_EQ(release_lzss_sparse_hash_tree_bucket(
                  fixture.context(), result.root, result.node_count),
              LzssSparseHashTreeBucketBuildError::invalid_tree);
    EXPECT_EQ(snapshot(fixture.pool), before_release);
}

} // namespace

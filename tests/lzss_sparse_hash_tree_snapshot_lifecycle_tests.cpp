#include "dictionary/lzss_sparse_hash_tree_bucket_builder.hpp"
#include "dictionary/lzss_sparse_hash_tree_snapshot_lifecycle.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] std::vector<std::byte> bytes(
    const std::string_view text) {
    std::vector<std::byte> result{};
    result.reserve(text.size());
    for (const auto value : text) {
        result.push_back(
            static_cast<std::byte>(static_cast<unsigned char>(value)));
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

struct SnapshotLifecycleFixture {
    std::vector<std::byte> input{bytes(
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")};
    LzssParameters parameters{};
    std::vector<std::uint32_t> links{};
    AlignedStorage storage{};
    LzssSparseHashTreeNodePool pool{};
    LzssSparseHashTreeBucketBuildResult built{};

    SnapshotLifecycleFixture() {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        links.assign(parameters.window_size, 0);
        links[10] = 5;
        links[5] = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, 3);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                      input.size(), parameters, {}, 3,
                      storage.bytes.first(required.workspace_size), pool),
                  LzssSparseHashTreeError::none);
        built = build_lzss_sparse_hash_tree_bucket({
            input, parameters, 15, 0, 1, 10, links, &pool, nullptr});
        EXPECT_EQ(built.error,
                  LzssSparseHashTreeBucketBuildError::none);
        EXPECT_EQ(built.node_count, 3U);
    }

    [[nodiscard]] LzssHashTreeBucketQueryContext snapshot_context() {
        const auto nodes = pool.node_arrays();
        return {input, parameters, 35, 0, 1, built.root,
                nodes.left, nodes.right, nodes.parent, nodes.height,
                nodes.position, nodes.subtree_maximum_position, nullptr,
                LzssHashTreeNodeIdentity::pool_local};
    }

    [[nodiscard]] LzssSparseHashTreeSnapshotReleaseContext
    release_context() {
        return {snapshot_context(), built.node_count, &pool};
    }
};

TEST(LzssSparseHashTreeSnapshotLifecycle,
     ValidatesEveryNodeAfterWholeSnapshotExpires) {
    SnapshotLifecycleFixture fixture{};
    const auto validation = validate_lzss_hash_tree_snapshot(
        fixture.snapshot_context(), fixture.built.node_count);
    EXPECT_EQ(validation.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(validation.nodes_visited, 3U);

    const auto wrong_count = validate_lzss_hash_tree_snapshot(
        fixture.snapshot_context(), fixture.built.node_count - 1U);
    EXPECT_EQ(wrong_count.error,
              LzssHashTreeBucketQueryError::invalid_tree);
}

TEST(LzssSparseHashTreeSnapshotLifecycle,
     ReleasesWhollyExpiredSnapshotWithoutCurrentChain) {
    SnapshotLifecycleFixture fixture{};
    fixture.links.assign(fixture.links.size(), UINT32_C(0));
    const auto released = release_lzss_sparse_hash_tree_snapshot(
        fixture.release_context());
    EXPECT_EQ(released.error,
              LzssSparseHashTreeSnapshotReleaseError::none);
    EXPECT_EQ(released.released_node_count, 3U);
    EXPECT_EQ(fixture.pool.active_count(), 0U);
    EXPECT_EQ(fixture.pool.free_count(), 3U);
    EXPECT_TRUE(fixture.pool.state_valid());
}

TEST(LzssSparseHashTreeSnapshotLifecycle,
     CorruptionFailsBeforeAnyNodeIsReleased) {
    SnapshotLifecycleFixture fixture{};
    auto nodes = fixture.pool.node_arrays();
    nodes.subtree_maximum_position[fixture.built.root] = 9;
    const auto before_active = fixture.pool.active_count();
    const auto before_free = fixture.pool.free_count();
    const auto released = release_lzss_sparse_hash_tree_snapshot(
        fixture.release_context());
    EXPECT_EQ(released.error,
              LzssSparseHashTreeSnapshotReleaseError::validation_failure);
    EXPECT_EQ(released.validation_error,
              LzssHashTreeBucketQueryError::invalid_root);
    EXPECT_EQ(released.released_node_count, 0U);
    EXPECT_EQ(fixture.pool.active_count(), before_active);
    EXPECT_EQ(fixture.pool.free_count(), before_free);
}

TEST(LzssSparseHashTreeSnapshotLifecycle,
     MismatchedPoolArraysFailWithoutMutation) {
    SnapshotLifecycleFixture fixture{};
    auto context = fixture.release_context();
    std::vector<std::uint32_t> unrelated(
        context.snapshot.left.begin(), context.snapshot.left.end());
    context.snapshot.left = unrelated;
    const auto before_active = fixture.pool.active_count();
    const auto released = release_lzss_sparse_hash_tree_snapshot(context);
    EXPECT_EQ(released.error,
              LzssSparseHashTreeSnapshotReleaseError::invalid_context);
    EXPECT_EQ(released.released_node_count, 0U);
    EXPECT_EQ(fixture.pool.active_count(), before_active);
}

} // namespace
} // namespace marc::dictionary::internal

#include "dictionary/lzss_sparse_hash_tree_controller.hpp"

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

struct ControllerFixture {
    explicit ControllerFixture(const std::size_t capacity = 4)
        : input(bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")) {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, capacity);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                      input.size(), parameters, {}, capacity,
                      storage.bytes.first(required.workspace_size), workspace),
                  LzssSparseHashTreeError::none);
        const auto hash = calculate_lzss_prefix_hash(input, 0);
        EXPECT_TRUE(hash.valid);
        bucket = static_cast<std::size_t>(hash.value)
            & (workspace.heads().size() - 1U);
    }

    [[nodiscard]] LzssSparseHashTreePositionContext context() {
        return {input, parameters, &workspace, nullptr};
    }

    [[nodiscard]] LzssSparseHashTreeBucketBuildContext build_context(
        const std::size_t query_position) {
        return {input, parameters, query_position, bucket,
                workspace.heads().size(), workspace.heads()[bucket],
                workspace.links(), &workspace.node_pool(), nullptr};
    }

    std::vector<std::byte> input{};
    LzssParameters parameters{};
    AlignedStorage storage{};
    LzssSparseHashTreeWorkspace workspace{};
    std::size_t bucket{};
};

TEST(LzssSparseHashTreeController, InsertsCompleteChainDeterministically) {
    ControllerFixture fixture{};
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(fixture.context(), 0).error,
              LzssSparseHashTreeControllerError::none);
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(fixture.context(), 5).error,
              LzssSparseHashTreeControllerError::none);
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(fixture.context(), 10).error,
              LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], 10U);
    EXPECT_EQ(fixture.workspace.links()[0], 0U);
    EXPECT_EQ(fixture.workspace.links()[5], 5U);
    EXPECT_EQ(fixture.workspace.links()[10], 5U);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::chain);
}

TEST(LzssSparseHashTreeController,
     CommitsPromotionThenSynchronizesRetirementAndInsertion) {
    ControllerFixture fixture{4};
    for (const auto position : {0U, 5U, 10U}) {
        ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                      fixture.context(), position).error,
                  LzssSparseHashTreeControllerError::none);
    }
    const auto promoted = promote_lzss_sparse_hash_tree_bucket(
        fixture.build_context(15), LzssSparseHashTreeBucketMode::chain,
        lzss_hash_tree_null_node, 0);
    ASSERT_EQ(promoted.error,
              LzssSparseHashTreeBucketTransitionError::none);
    ASSERT_EQ(commit_lzss_sparse_hash_tree_bucket_transition(
                  fixture.workspace, fixture.bucket,
                  LzssSparseHashTreeBucketMode::chain,
                  lzss_hash_tree_null_node, 0, promoted),
              LzssSparseHashTreeControllerError::none);

    const auto advanced = insert_lzss_sparse_hash_tree_position(
        fixture.context(), 20);
    ASSERT_EQ(advanced.error, LzssSparseHashTreeControllerError::none);
    EXPECT_TRUE(advanced.retired);
    EXPECT_TRUE(advanced.inserted);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], 20U);
    EXPECT_EQ(fixture.workspace.links()[0], 10U);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 3U);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 3U);
}

TEST(LzssSparseHashTreeController,
     TerminalChainInsertionDoesNotRetryPromotion) {
    ControllerFixture fixture{1};
    fixture.workspace.modes()[fixture.bucket] =
        LzssSparseHashTreeBucketMode::pool_rejected_chain;
    const auto inserted = insert_lzss_sparse_hash_tree_position(
        fixture.context(), 0);
    EXPECT_EQ(inserted.error, LzssSparseHashTreeControllerError::none);
    EXPECT_TRUE(inserted.inserted);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
}

TEST(LzssSparseHashTreeController,
     StaleExpectedMetadataRejectsCommitAtomically) {
    ControllerFixture fixture{};
    LzssSparseHashTreeBucketTransitionResult transition{};
    transition.mode = LzssSparseHashTreeBucketMode::pool_rejected_chain;
    transition.status =
        LzssSparseHashTreeBucketTransitionStatus::pool_rejected_chain;
    fixture.workspace.modes()[fixture.bucket] =
        LzssSparseHashTreeBucketMode::promoted_tree;
    EXPECT_EQ(commit_lzss_sparse_hash_tree_bucket_transition(
                  fixture.workspace, fixture.bucket,
                  LzssSparseHashTreeBucketMode::chain,
                  lzss_hash_tree_null_node, 0, transition),
              LzssSparseHashTreeControllerError::invalid_metadata);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
}

TEST(LzssSparseHashTreeController,
     InvalidBucketMetadataPreventsChainWrite) {
    ControllerFixture fixture{};
    fixture.workspace.roots()[fixture.bucket] = 0;
    const auto before_head = fixture.workspace.heads()[fixture.bucket];
    const auto before_link = fixture.workspace.links()[0];
    const auto inserted = insert_lzss_sparse_hash_tree_position(
        fixture.context(), 0);
    EXPECT_EQ(inserted.error,
              LzssSparseHashTreeControllerError::invalid_metadata);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], before_head);
    EXPECT_EQ(fixture.workspace.links()[0], before_link);
}

TEST(LzssSparseHashTreeController,
     InconsistentSuccessfulTransitionCannotCommit) {
    ControllerFixture fixture{};
    LzssSparseHashTreeBucketTransitionResult transition{};
    transition.status = LzssSparseHashTreeBucketTransitionStatus::inserted;
    transition.mode = LzssSparseHashTreeBucketMode::chain;
    const auto before_mode = fixture.workspace.modes()[fixture.bucket];
    EXPECT_EQ(commit_lzss_sparse_hash_tree_bucket_transition(
                  fixture.workspace, fixture.bucket,
                  LzssSparseHashTreeBucketMode::chain,
                  lzss_hash_tree_null_node, 0, transition),
              LzssSparseHashTreeControllerError::commit_failure);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket], before_mode);
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket],
              lzss_hash_tree_null_node);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 0U);
}

TEST(LzssSparseHashTreeController,
     InvalidPositionDoesNotChangeWorkspace) {
    ControllerFixture fixture{};
    const auto before_head = fixture.workspace.heads()[fixture.bucket];
    const auto inserted = insert_lzss_sparse_hash_tree_position(
        fixture.context(), fixture.input.size() - 1U);
    EXPECT_EQ(inserted.error,
              LzssSparseHashTreeControllerError::invalid_position);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], before_head);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
}

} // namespace

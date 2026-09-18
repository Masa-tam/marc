#include "dictionary/lzss_sparse_hash_tree_snapshot_controller.hpp"

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

struct SnapshotControllerFixture {
    std::vector<std::byte> input{bytes(
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")};
    LzssParameters parameters{};
    AlignedStorage storage{};
    LzssSparseHashTreeWorkspace workspace{};
    LzssSparseHashTreeSnapshotControllerState state{};
    LzssMatchFinderStatistics statistics{};
    LzssHashTreePromotionState promotion{};
    std::size_t bucket{};

    explicit SnapshotControllerFixture(
        const std::size_t delta_candidate_budget = 0) {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, 15);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                      input.size(), parameters, {}, 15,
                      storage.bytes.first(required.workspace_size),
                      workspace),
                  LzssSparseHashTreeError::none);
        initialize_lzss_sparse_hash_tree_snapshot_controller_state(
            input.size(), workspace.heads().size(), state,
            delta_candidate_budget);
        const auto hash = calculate_lzss_prefix_hash(input, 0);
        EXPECT_TRUE(hash.valid);
        bucket = static_cast<std::size_t>(hash.value)
            & (workspace.heads().size() - 1U);
    }

    [[nodiscard]] LzssSparseHashTreePositionContext context() {
        return {input, parameters, &workspace, &statistics, nullptr};
    }

    [[nodiscard]] LzssSparseHashTreePositionContext promotion_context() {
        return {input, parameters, &workspace, &statistics, &promotion};
    }

    void seed_and_promote() {
        const auto seeded =
            advance_lzss_sparse_hash_tree_snapshot_controller(
                context(), state, 0, 15);
        ASSERT_EQ(seeded.error,
                  LzssSparseHashTreeSnapshotControllerError::none);
        const auto promoted = promote_lzss_sparse_hash_tree_bucket(
            {input, parameters, 15, bucket, workspace.heads().size(),
             workspace.heads()[bucket], workspace.links(),
             &workspace.node_pool(), nullptr},
            LzssSparseHashTreeBucketMode::chain,
            lzss_hash_tree_null_node, 0);
        ASSERT_EQ(promoted.error,
                  LzssSparseHashTreeBucketTransitionError::none);
        ASSERT_EQ(promoted.node_count, 15U);
        ASSERT_EQ(commit_lzss_sparse_hash_tree_bucket_transition(
                      workspace, bucket,
                      LzssSparseHashTreeBucketMode::chain,
                      lzss_hash_tree_null_node, 0, promoted),
                  LzssSparseHashTreeControllerError::none);
    }
};

struct DeltaBudgetControllerFixture {
    std::vector<std::byte> input{bytes(
        "AAAAAaAAAAAbAAAAAcAAAAAdAAAAAeAAAAAfAAAAAgAAAAAh")};
    LzssParameters parameters{};
    AlignedStorage storage{};
    LzssSparseHashTreeWorkspace workspace{};
    LzssSparseHashTreeSnapshotControllerState state{};
    LzssMatchFinderStatistics statistics{};
    std::size_t bucket{};

    explicit DeltaBudgetControllerFixture(
        const std::size_t delta_candidate_budget,
        const std::uint32_t window_size = 40) {
        parameters.window_size = window_size;
        parameters.max_match_length = 6;
        const auto required = calculate_lzss_sparse_hash_tree_workspace(
            input.size(), parameters, {}, 16);
        EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
        storage = make_storage(required.workspace_size);
        EXPECT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                      input.size(), parameters, {}, 16,
                      storage.bytes.first(required.workspace_size),
                      workspace),
                  LzssSparseHashTreeError::none);
        initialize_lzss_sparse_hash_tree_snapshot_controller_state(
            input.size(), workspace.heads().size(), state,
            delta_candidate_budget);
        const auto hash = calculate_lzss_prefix_hash(input, 0);
        EXPECT_TRUE(hash.valid);
        bucket = static_cast<std::size_t>(hash.value)
            & (workspace.heads().size() - 1U);

        EXPECT_EQ(advance_lzss_sparse_hash_tree_snapshot_controller(
                      context(), state, 0, 18).error,
                  LzssSparseHashTreeSnapshotControllerError::none);
        const auto promoted = promote_lzss_sparse_hash_tree_bucket(
            {input, parameters, 18, bucket, workspace.heads().size(),
             workspace.heads()[bucket], workspace.links(),
             &workspace.node_pool(), nullptr},
            LzssSparseHashTreeBucketMode::chain,
            lzss_hash_tree_null_node, 0);
        EXPECT_EQ(promoted.error,
                  LzssSparseHashTreeBucketTransitionError::none);
        EXPECT_EQ(promoted.status,
                  LzssSparseHashTreeBucketTransitionStatus::promoted);
        EXPECT_EQ(commit_lzss_sparse_hash_tree_bucket_transition(
                      workspace, bucket,
                      LzssSparseHashTreeBucketMode::chain,
                      lzss_hash_tree_null_node, 0, promoted),
                  LzssSparseHashTreeControllerError::none);
        EXPECT_EQ(advance_lzss_sparse_hash_tree_snapshot_controller(
                      context(), state, 18, 36).error,
                  LzssSparseHashTreeSnapshotControllerError::none);
    }

    [[nodiscard]] LzssSparseHashTreePositionContext context() {
        return {input, parameters, &workspace, &statistics, nullptr};
    }

    [[nodiscard]] LzssSparseHashTreeSnapshotControllerQueryResult query() {
        return query_lzss_sparse_hash_tree_snapshot_controller_exact(
            context(), state, 36);
    }
};

TEST(LzssSparseHashTreeSnapshotController,
     PromotionLifetimeUsesOnlyChainUpdates) {
    SnapshotControllerFixture fixture{};
    fixture.seed_and_promote();
    const auto root = fixture.workspace.roots()[fixture.bucket];
    const auto count = fixture.workspace.bucket_node_counts()[fixture.bucket];
    const auto active = fixture.workspace.node_pool().active_count();

    const auto query =
        query_lzss_sparse_hash_tree_snapshot_controller_exact(
            fixture.context(), fixture.state, 15);
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(query.match, (LzssMatch{1, 5}));
    EXPECT_FALSE(query.snapshot_expired);
    EXPECT_EQ(fixture.statistics.query_count, 1U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_query_count, 1U);
    EXPECT_GT(fixture.statistics.hash_tree_snapshot_query_node_count, 0U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_delta_query_count, 1U);

    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 15, 35);
    ASSERT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(advanced.positions_inserted, 20U);
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket], root);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], count);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), active);
    EXPECT_EQ(fixture.statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(fixture.statistics.hash_tree_retirement_count, 0U);
}

TEST(LzssSparseHashTreeSnapshotController,
     ExpiredQuerySchedulesReleaseOnNextAdvance) {
    SnapshotControllerFixture fixture{};
    fixture.seed_and_promote();
    ASSERT_EQ(advance_lzss_sparse_hash_tree_snapshot_controller(
                  fixture.context(), fixture.state, 15, 35).error,
              LzssSparseHashTreeSnapshotControllerError::none);

    const auto query =
        query_lzss_sparse_hash_tree_snapshot_controller_exact(
            fixture.context(), fixture.state, 35);
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_TRUE(query.snapshot_expired);
    EXPECT_EQ(query.match, (LzssMatch{1, 5}));
    EXPECT_EQ(query.snapshot_stale_subtrees_pruned, 1U);
    EXPECT_EQ(fixture.state.pending_release_bucket(), fixture.bucket);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 15U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_expiration_count, 1U);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_stale_subtree_prune_count,
        1U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_delta_candidate_count,
              query.delta_candidates_visited);
    EXPECT_EQ(
        fixture.statistics
            .hash_tree_snapshot_delta_maximum_candidates_per_query,
        query.delta_candidates_visited);

    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 35, 36);
    ASSERT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(advanced.released_bucket, fixture.bucket);
    EXPECT_EQ(advanced.released_node_count, 15U);
    EXPECT_EQ(fixture.state.pending_release_bucket(),
              lzss_sparse_hash_tree_no_pending_snapshot_bucket);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::chain);
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket],
              lzss_hash_tree_null_node);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 0U);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], 35U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_bulk_release_count, 1U);
}

TEST(LzssSparseHashTreeSnapshotController,
     ReleaseFailurePreservesBucketMetadataAndPoisonsState) {
    SnapshotControllerFixture fixture{};
    fixture.seed_and_promote();
    ASSERT_EQ(advance_lzss_sparse_hash_tree_snapshot_controller(
                  fixture.context(), fixture.state, 15, 35).error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_TRUE(query_lzss_sparse_hash_tree_snapshot_controller_exact(
                    fixture.context(), fixture.state, 35)
                    .snapshot_expired);
    const auto root = fixture.workspace.roots()[fixture.bucket];
    const auto count = fixture.workspace.bucket_node_counts()[fixture.bucket];
    auto nodes = fixture.workspace.node_pool().node_arrays();
    nodes.subtree_maximum_position[root] = 13;

    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 35, 36);
    EXPECT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::release_failure);
    EXPECT_EQ(advanced.release_error,
              LzssSparseHashTreeSnapshotReleaseError::validation_failure);
    EXPECT_FALSE(fixture.state.state_valid());
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket], root);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], count);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 15U);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], 34U);
}

TEST(LzssSparseHashTreeSnapshotController,
     InvalidAdvanceProtocolDoesNotMutateWorkspace) {
    SnapshotControllerFixture fixture{};
    const auto before_head = fixture.workspace.heads()[fixture.bucket];
    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 1, 2);
    EXPECT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::invalid_protocol);
    EXPECT_FALSE(fixture.state.state_valid());
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], before_head);
}

TEST(LzssSparseHashTreeSnapshotController,
     StatisticsObserveDedicatedPromotionWithoutTreeMutation) {
    SnapshotControllerFixture fixture{};
    initialize_lzss_hash_tree_promotion_state(
        fixture.workspace.heads().size(), 0, fixture.promotion);
    ASSERT_EQ(advance_lzss_sparse_hash_tree_snapshot_controller(
                  fixture.promotion_context(), fixture.state, 0, 15).error,
              LzssSparseHashTreeSnapshotControllerError::none);

    const auto chain =
        query_lzss_sparse_hash_tree_snapshot_controller_exact(
            fixture.promotion_context(), fixture.state, 15);
    ASSERT_EQ(chain.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_EQ(fixture.promotion.phase(),
              LzssHashTreePromotionPhase::pending);
    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.promotion_context(), fixture.state, 15, 16);
    ASSERT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_promotion_count, 1U);
    EXPECT_EQ(fixture.statistics.hash_tree_promotion_count, 1U);
    EXPECT_EQ(fixture.statistics.hash_tree_promotion_build_node_count, 15U);
    EXPECT_GT(
        fixture.statistics.hash_tree_promotion_build_key_comparison_count,
        0U);
    EXPECT_GT(fixture.statistics.hash_tree_promotion_build_rotation_count,
              0U);
    EXPECT_EQ(fixture.statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(fixture.statistics.hash_tree_retirement_count, 0U);
    EXPECT_FALSE(fixture.statistics.overflowed);
}

TEST(LzssSparseHashTreeSnapshotController,
     StatisticsSaturateWithoutChangingMatch) {
    SnapshotControllerFixture fixture{};
    fixture.seed_and_promote();
    fixture.statistics.hash_tree_snapshot_query_count = UINT64_MAX;
    const auto query =
        query_lzss_sparse_hash_tree_snapshot_controller_exact(
            fixture.context(), fixture.state, 15);
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(query.match, (LzssMatch{1, 5}));
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_query_count,
              UINT64_MAX);
    EXPECT_TRUE(fixture.statistics.overflowed);
}

TEST(LzssSparseHashTreeSnapshotController,
     DeltaBudgetUsesStrictBoundaryAndZeroDisablesPolicy) {
    DeltaBudgetControllerFixture disabled{0};
    const auto disabled_query = disabled.query();
    ASSERT_EQ(disabled_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    const auto candidate_count = disabled_query.delta_candidates_visited;
    ASSERT_GT(candidate_count, 1U);
    EXPECT_FALSE(disabled_query.delta_budget_exceeded);
    EXPECT_EQ(disabled.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::none);
    EXPECT_EQ(
        disabled.statistics.hash_tree_snapshot_delta_budget_query_count,
        0U);

    DeltaBudgetControllerFixture below{candidate_count - 1U};
    const auto below_query = below.query();
    ASSERT_EQ(below_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_EQ(below_query.delta_candidates_visited, candidate_count);
    EXPECT_TRUE(below_query.delta_budget_exceeded);
    EXPECT_EQ(below.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::delta_budget);
    EXPECT_EQ(
        below.statistics.hash_tree_snapshot_delta_budget_query_count, 1U);
    EXPECT_EQ(
        below.statistics.hash_tree_snapshot_delta_budget_breach_count, 1U);
    EXPECT_EQ(
        below.statistics
            .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach,
        candidate_count);
    const auto repeated_below_query = below.query();
    ASSERT_EQ(repeated_below_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_TRUE(repeated_below_query.delta_budget_exceeded);
    EXPECT_EQ(
        below.statistics.hash_tree_snapshot_delta_budget_query_count, 2U);
    EXPECT_EQ(
        below.statistics.hash_tree_snapshot_delta_budget_breach_count, 1U);

    DeltaBudgetControllerFixture equal{candidate_count};
    const auto equal_query = equal.query();
    ASSERT_EQ(equal_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_FALSE(equal_query.delta_budget_exceeded);
    EXPECT_EQ(equal.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::none);

    DeltaBudgetControllerFixture above{candidate_count + 1U};
    const auto above_query = above.query();
    ASSERT_EQ(above_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_FALSE(above_query.delta_budget_exceeded);
    EXPECT_EQ(above.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::none);
}

TEST(LzssSparseHashTreeSnapshotController,
     DeltaBudgetDemotesAfterExactQueryAndPreventsRepromotion) {
    DeltaBudgetControllerFixture baseline{0};
    const auto baseline_query = baseline.query();
    ASSERT_EQ(baseline_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_GT(baseline_query.delta_candidates_visited, 1U);

    DeltaBudgetControllerFixture fixture{
        baseline_query.delta_candidates_visited - 1U};
    const auto query = fixture.query();
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(query.match, baseline_query.match);
    ASSERT_TRUE(query.delta_budget_exceeded);
    EXPECT_EQ(fixture.state.pending_release_bucket(), fixture.bucket);

    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 36, 42);
    ASSERT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(advanced.release_reason,
              LzssSparseHashTreeSnapshotReleaseReason::delta_budget);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket],
              lzss_hash_tree_null_node);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 0U);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_bulk_release_count, 1U);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_demotion_count,
        1U);

    const auto following =
        query_lzss_sparse_hash_tree_snapshot_controller_exact(
            fixture.context(), fixture.state, 42);
    ASSERT_EQ(following.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_query_count, 1U);
    EXPECT_GT(fixture.statistics.hash_tree_chain_query_count, 0U);
}

TEST(LzssSparseHashTreeSnapshotController,
     DeltaBudgetStatisticsSaturateWithoutChangingMatch) {
    DeltaBudgetControllerFixture baseline{0};
    const auto baseline_query = baseline.query();
    ASSERT_EQ(baseline_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_GT(baseline_query.delta_candidates_visited, 1U);

    DeltaBudgetControllerFixture fixture{
        baseline_query.delta_candidates_visited - 1U};
    fixture.statistics.hash_tree_snapshot_delta_budget_query_count =
        UINT64_MAX;
    fixture.statistics.hash_tree_snapshot_delta_budget_breach_count =
        UINT64_MAX;
    const auto query = fixture.query();
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_EQ(query.match, baseline_query.match);
    EXPECT_TRUE(query.delta_budget_exceeded);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_query_count,
        UINT64_MAX);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_breach_count,
        UINT64_MAX);
    EXPECT_TRUE(fixture.statistics.overflowed);
}

TEST(LzssSparseHashTreeSnapshotController,
     DeltaBudgetDominatesSimultaneousExpiration) {
    DeltaBudgetControllerFixture baseline{0, 20};
    const auto baseline_query = baseline.query();
    ASSERT_EQ(baseline_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_TRUE(baseline_query.snapshot_expired);
    ASSERT_GT(baseline_query.delta_candidates_visited, 1U);

    DeltaBudgetControllerFixture fixture{
        baseline_query.delta_candidates_visited - 1U, 20};
    const auto query = fixture.query();
    ASSERT_EQ(query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    EXPECT_TRUE(query.snapshot_expired);
    EXPECT_TRUE(query.delta_budget_exceeded);
    EXPECT_EQ(fixture.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::delta_budget);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_expiration_count, 0U);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_breach_count, 1U);
}

TEST(LzssSparseHashTreeSnapshotController,
     DeltaBudgetReleaseFailureDoesNotCommitDemotion) {
    DeltaBudgetControllerFixture baseline{0};
    const auto baseline_query = baseline.query();
    ASSERT_EQ(baseline_query.error,
              LzssSparseHashTreeSnapshotControllerError::none);
    ASSERT_GT(baseline_query.delta_candidates_visited, 1U);

    DeltaBudgetControllerFixture fixture{
        baseline_query.delta_candidates_visited - 1U};
    ASSERT_TRUE(fixture.query().delta_budget_exceeded);
    const auto root = fixture.workspace.roots()[fixture.bucket];
    const auto count = fixture.workspace.bucket_node_counts()[fixture.bucket];
    auto nodes = fixture.workspace.node_pool().node_arrays();
    nodes.subtree_maximum_position[root] = 0;

    const auto advanced =
        advance_lzss_sparse_hash_tree_snapshot_controller(
            fixture.context(), fixture.state, 36, 42);
    EXPECT_EQ(advanced.error,
              LzssSparseHashTreeSnapshotControllerError::release_failure);
    EXPECT_FALSE(fixture.state.state_valid());
    EXPECT_EQ(fixture.state.pending_release_reason(),
              LzssSparseHashTreeSnapshotReleaseReason::delta_budget);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.workspace.roots()[fixture.bucket], root);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], count);
    EXPECT_EQ(fixture.statistics.hash_tree_snapshot_bulk_release_count, 0U);
    EXPECT_EQ(
        fixture.statistics.hash_tree_snapshot_delta_budget_demotion_count,
        0U);
}

} // namespace
} // namespace marc::dictionary::internal

#include "dictionary/lzss_sparse_hash_tree_controller.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <tuple>
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

    [[nodiscard]] LzssSparseHashTreePositionContext promotion_context() {
        return {input, parameters, &workspace, nullptr, &promotion};
    }

    void initialize_promotion(const std::uint64_t threshold) {
        initialize_lzss_hash_tree_promotion_state(
            workspace.heads().size(), threshold, promotion);
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
    LzssHashTreePromotionState promotion{};
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
     PrefixlessTailRetiresWithoutInsertion) {
    ControllerFixture fixture{};
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                  fixture.context(), 0).error,
              LzssSparseHashTreeControllerError::none);
    const auto inserted = insert_lzss_sparse_hash_tree_position(
        fixture.context(), fixture.input.size() - 1U);
    EXPECT_EQ(inserted.error, LzssSparseHashTreeControllerError::none);
    EXPECT_FALSE(inserted.inserted);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
}

TEST(LzssSparseHashTreeController,
     PrefixlessTailRetiresPromotedNode) {
    ControllerFixture fixture{};
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                  fixture.context(), 10).error,
              LzssSparseHashTreeControllerError::none);
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
    ASSERT_EQ(fixture.workspace.node_pool().active_count(), 1U);

    const auto advanced = insert_lzss_sparse_hash_tree_position(
        fixture.context(), 30);
    EXPECT_EQ(advanced.error, LzssSparseHashTreeControllerError::none);
    EXPECT_TRUE(advanced.retired);
    EXPECT_FALSE(advanced.inserted);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 0U);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
}

TEST(LzssSparseHashTreeController, ChainQueryReturnsNearestExactMatch) {
    ControllerFixture fixture{};
    for (const auto position : {0U, 5U, 10U}) {
        ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                      fixture.context(), position).error,
                  LzssSparseHashTreeControllerError::none);
    }
    const auto query = query_lzss_sparse_hash_tree_exact(
        fixture.context(), 15);
    EXPECT_EQ(query.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(query.source, LzssSparseHashTreeQuerySource::chain);
    EXPECT_EQ(query.bucket, fixture.bucket);
    EXPECT_EQ(query.match, (LzssMatch{5, 5}));
    EXPECT_EQ(query.candidate_count, 1U);
}

TEST(LzssSparseHashTreeController,
     PendingThresholdPromotesBeforePositionInsertion) {
    ControllerFixture fixture{4};
    for (const auto position : {0U, 5U, 10U}) {
        ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                      fixture.context(), position).error,
                  LzssSparseHashTreeControllerError::none);
    }
    fixture.initialize_promotion(0);
    const auto chain = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 15);
    ASSERT_EQ(chain.error, LzssSparseHashTreeControllerError::none);
    ASSERT_EQ(fixture.promotion.phase(),
              LzssHashTreePromotionPhase::pending);

    const auto inserted = insert_lzss_sparse_hash_tree_position(
        fixture.promotion_context(), 15);
    ASSERT_EQ(inserted.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(fixture.promotion.phase(), LzssHashTreePromotionPhase::idle);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(fixture.workspace.bucket_node_counts()[fixture.bucket], 4U);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 4U);

    const auto tree = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 20);
    EXPECT_EQ(tree.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(tree.source, LzssSparseHashTreeQuerySource::pool_tree);
    EXPECT_EQ(tree.match, chain.match);
}

TEST(LzssSparseHashTreeController,
     ChainAndPromotedTreeAgreeAtSameQueryPosition) {
    ControllerFixture fixture{4};
    for (const auto position : {0U, 5U, 10U}) {
        ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                      fixture.context(), position).error,
                  LzssSparseHashTreeControllerError::none);
    }
    fixture.initialize_promotion(0);
    const auto chain = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 15);
    ASSERT_EQ(chain.error, LzssSparseHashTreeControllerError::none);
    const auto promotion = promote_pending_lzss_sparse_hash_tree_bucket(
        fixture.promotion_context(), 15);
    ASSERT_EQ(promotion.error, LzssSparseHashTreeControllerError::none);
    ASSERT_TRUE(promotion.promoted);
    const auto tree = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 15);
    EXPECT_EQ(tree.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(tree.source, LzssSparseHashTreeQuerySource::pool_tree);
    EXPECT_EQ(tree.bucket, chain.bucket);
    EXPECT_EQ(tree.match, chain.match);
}

TEST(LzssSparseHashTreeController,
     PromotionCapacityFailureBecomesTerminalChain) {
    ControllerFixture fixture{2};
    for (const auto position : {0U, 5U, 10U}) {
        ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                      fixture.context(), position).error,
                  LzssSparseHashTreeControllerError::none);
    }
    fixture.initialize_promotion(0);
    ASSERT_EQ(query_lzss_sparse_hash_tree_exact(
                  fixture.promotion_context(), 15).error,
              LzssSparseHashTreeControllerError::none);
    ASSERT_EQ(insert_lzss_sparse_hash_tree_position(
                  fixture.promotion_context(), 15).error,
              LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(fixture.workspace.modes()[fixture.bucket],
              LzssSparseHashTreeBucketMode::pool_rejected_chain);
    EXPECT_EQ(fixture.workspace.node_pool().active_count(), 0U);
    EXPECT_EQ(fixture.promotion.phase(), LzssHashTreePromotionPhase::idle);

    const auto query = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 20);
    EXPECT_EQ(query.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(query.source, LzssSparseHashTreeQuerySource::chain);
    EXPECT_EQ(query.match, (LzssMatch{5, 5}));
    EXPECT_EQ(fixture.promotion.phase(), LzssHashTreePromotionPhase::idle);
}

TEST(LzssSparseHashTreeController,
     CorruptPromotedMetadataRejectsQuery) {
    ControllerFixture fixture{};
    fixture.workspace.modes()[fixture.bucket] =
        LzssSparseHashTreeBucketMode::promoted_tree;
    fixture.workspace.roots()[fixture.bucket] = 0;
    const auto query = query_lzss_sparse_hash_tree_exact(
        fixture.context(), 5);
    EXPECT_EQ(query.error,
              LzssSparseHashTreeControllerError::invalid_metadata);
    EXPECT_EQ(query.source, LzssSparseHashTreeQuerySource::none);
}

TEST(LzssSparseHashTreeController, PrefixlessTailQueryIsEmpty) {
    ControllerFixture fixture{};
    const auto query = query_lzss_sparse_hash_tree_exact(
        fixture.context(), fixture.input.size() - 1U);
    EXPECT_EQ(query.error, LzssSparseHashTreeControllerError::none);
    EXPECT_EQ(query.source, LzssSparseHashTreeQuerySource::none);
    EXPECT_EQ(query.match, LzssMatch{});
}

TEST(LzssSparseHashTreeController,
     MismatchedPromotionStateRejectsContext) {
    ControllerFixture fixture{};
    initialize_lzss_hash_tree_promotion_state(
        fixture.workspace.heads().size() / 2U, 0, fixture.promotion);
    const auto query = query_lzss_sparse_hash_tree_exact(
        fixture.promotion_context(), 5);
    EXPECT_EQ(query.error,
              LzssSparseHashTreeControllerError::invalid_context);
}

TEST(LzssSparseHashTreeController,
     MultiPositionAdvanceMatchesExhaustiveTokenBoundaries) {
    const auto input = bytes(
        "abracadabra abracadabra -- abracadabra -- xyzxyzxyzxyz -- "
        "abracadabra");
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 12;
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        input.size(), parameters, {}, parameters.window_size);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeWorkspace workspace{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_workspace(
                  input.size(), parameters, {}, parameters.window_size,
                  storage.bytes.first(required.workspace_size), workspace),
              LzssSparseHashTreeError::none);
    LzssHashTreePromotionState promotion{};
    initialize_lzss_hash_tree_promotion_state(
        workspace.heads().size(), 0, promotion);
    LzssSparseHashTreeAdvanceState advance_state{};
    initialize_lzss_sparse_hash_tree_advance_state(
        input.size(), advance_state);
    LzssSparseHashTreePositionContext context{
        input, parameters, &workspace, nullptr, &promotion};
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::vector<std::tuple<std::size_t, LzssMatch, bool>> sparse_tokens{};
    std::vector<std::tuple<std::size_t, LzssMatch, bool>> reference_tokens{};
    std::size_t position{};
    while (position < input.size()) {
        const auto sparse = query_lzss_sparse_hash_tree_exact(
            context, position);
        ASSERT_EQ(sparse.error, LzssSparseHashTreeControllerError::none);
        const auto reference = exhaustive.find_match(position);
        const bool sparse_match = lzss_match_is_beneficial(sparse.match);
        const bool reference_match = lzss_match_is_beneficial(reference);
        sparse_tokens.emplace_back(position, sparse.match, sparse_match);
        reference_tokens.emplace_back(position, reference, reference_match);
        ASSERT_EQ(sparse.match, reference);
        ASSERT_EQ(sparse_match, reference_match);
        const auto consumed = sparse_match
            ? static_cast<std::size_t>(sparse.match.length) : 1U;
        const auto advanced = advance_lzss_sparse_hash_tree_positions(
            context, advance_state, position, position + consumed);
        ASSERT_EQ(advanced.error,
                  LzssSparseHashTreeControllerError::none);
        exhaustive.advance(position, position + consumed);
        position += consumed;
    }
    EXPECT_EQ(sparse_tokens, reference_tokens);
    EXPECT_EQ(advance_state.next_position(), input.size());
    EXPECT_TRUE(advance_state.state_valid());
}

TEST(LzssSparseHashTreeController,
     InvalidAdvanceProtocolPoisonsCursorWithoutMutation) {
    ControllerFixture fixture{};
    fixture.initialize_promotion(0);
    LzssSparseHashTreeAdvanceState state{};
    initialize_lzss_sparse_hash_tree_advance_state(
        fixture.input.size(), state);
    const auto before_head = fixture.workspace.heads()[fixture.bucket];
    const auto advanced = advance_lzss_sparse_hash_tree_positions(
        fixture.promotion_context(), state, 1, 2);
    EXPECT_EQ(advanced.error,
              LzssSparseHashTreeControllerError::invalid_protocol);
    EXPECT_FALSE(state.state_valid());
    EXPECT_EQ(state.last_error(),
              LzssSparseHashTreeControllerError::invalid_protocol);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], before_head);
    const auto repeated = advance_lzss_sparse_hash_tree_positions(
        fixture.promotion_context(), state, 0, 1);
    EXPECT_EQ(repeated.error,
              LzssSparseHashTreeControllerError::invalid_protocol);
    EXPECT_EQ(fixture.workspace.heads()[fixture.bucket], before_head);
}

} // namespace

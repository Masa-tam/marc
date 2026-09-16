#include "dictionary/lzss_hash_tree_promotion.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

[[nodiscard]] LzssHashTreePromotionState make_state(
    const std::uint64_t threshold) {
    LzssHashTreePromotionState state{};
    initialize_lzss_hash_tree_promotion_state(8, threshold, state);
    return state;
}

TEST(LzssHashTreePromotion, UsesStrictCandidateThreshold) {
    auto below = make_state(63);
    EXPECT_TRUE(below.record_completed_chain_query(2, 64).pending);
    EXPECT_EQ(below.phase(), LzssHashTreePromotionPhase::pending);

    auto equal = make_state(64);
    EXPECT_FALSE(equal.record_completed_chain_query(2, 64).pending);
    EXPECT_EQ(equal.phase(), LzssHashTreePromotionPhase::idle);

    auto above = make_state(65);
    EXPECT_FALSE(above.record_completed_chain_query(2, 64).pending);
    EXPECT_EQ(above.phase(), LzssHashTreePromotionPhase::idle);
}

TEST(LzssHashTreePromotion, ThresholdZeroStillRejectsEmptyQuery) {
    auto state = make_state(0);
    EXPECT_FALSE(state.record_completed_chain_query(3, 0).pending);
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::idle);
    EXPECT_TRUE(state.record_completed_chain_query(3, 1).pending);
    EXPECT_EQ(state.active_bucket(), 3U);
    EXPECT_EQ(state.trigger_candidate_count(), 1U);
}

TEST(LzssHashTreePromotion, MaximumThresholdDisablesPromotion) {
    auto state = make_state(std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(state.record_completed_chain_query(
                          3, std::numeric_limits<std::uint64_t>::max())
                     .pending);
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::idle);
    EXPECT_TRUE(state.state_valid());
}

TEST(LzssHashTreePromotion, RepeatedCompletedQueryIsIdempotent) {
    auto state = make_state(10);
    EXPECT_TRUE(state.record_completed_chain_query(4, 11).pending);
    EXPECT_TRUE(state.record_completed_chain_query(4, 11).pending);
    EXPECT_TRUE(state.state_valid());
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::pending);
    EXPECT_EQ(state.active_bucket(), 4U);
    EXPECT_EQ(state.trigger_candidate_count(), 11U);
}

TEST(LzssHashTreePromotion, RequiresRepeatedExpensiveBucketQueries) {
    LzssHashTreePromotionState state{};
    initialize_lzss_hash_tree_promotion_state(4, 10, state, 3);
    std::vector<std::uint8_t> counts(4);

    EXPECT_FALSE(state.record_completed_chain_query(2, 11, counts).pending);
    EXPECT_EQ(counts, (std::vector<std::uint8_t>{0, 0, 1, 0}));
    EXPECT_FALSE(state.record_completed_chain_query(1, 11, counts).pending);
    EXPECT_EQ(counts, (std::vector<std::uint8_t>{0, 1, 1, 0}));
    EXPECT_FALSE(state.record_completed_chain_query(2, 10, counts).pending);
    EXPECT_EQ(counts, (std::vector<std::uint8_t>{0, 1, 0, 0}));
    EXPECT_FALSE(state.record_completed_chain_query(2, 12, counts).pending);
    EXPECT_FALSE(state.record_completed_chain_query(2, 13, counts).pending);
    EXPECT_TRUE(state.record_completed_chain_query(2, 14, counts).pending);
    EXPECT_EQ(counts, (std::vector<std::uint8_t>{0, 1, 3, 0}));
    ASSERT_TRUE(state.begin_advance().required);
    EXPECT_EQ(state.commit(2, counts), LzssHashTreePromotionError::none);
    EXPECT_EQ(counts, (std::vector<std::uint8_t>{0, 1, 0, 0}));
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::idle);
}

TEST(LzssHashTreePromotion, MaximumReuseThresholdCannotWrap) {
    LzssHashTreePromotionState state{};
    initialize_lzss_hash_tree_promotion_state(
        1, 0, state, std::numeric_limits<std::uint8_t>::max());
    std::array<std::uint8_t, 1> counts{};
    for (std::size_t query = 1;
         query < std::numeric_limits<std::uint8_t>::max(); ++query) {
        EXPECT_FALSE(state.record_completed_chain_query(0, 1, counts).pending);
    }
    EXPECT_EQ(counts[0], UINT8_MAX - 1U);
    EXPECT_TRUE(state.record_completed_chain_query(0, 1, counts).pending);
    EXPECT_EQ(counts[0], UINT8_MAX);
    EXPECT_TRUE(state.record_completed_chain_query(0, 1, counts).pending);
    EXPECT_EQ(counts[0], UINT8_MAX);
}

TEST(LzssHashTreePromotion, InvalidReuseStateIsStickyAndAtomic) {
    LzssHashTreePromotionState invalid_threshold{};
    initialize_lzss_hash_tree_promotion_state(
        4, 10, invalid_threshold, 0);
    EXPECT_FALSE(invalid_threshold.state_valid());
    EXPECT_EQ(invalid_threshold.last_error(),
              LzssHashTreePromotionError::invalid_reuse_threshold);

    LzssHashTreePromotionState state{};
    initialize_lzss_hash_tree_promotion_state(4, 10, state, 2);
    std::array<std::uint8_t, 3> short_counts{1, 2, 3};
    const auto record = state.record_completed_chain_query(
        1, 11, short_counts);
    EXPECT_EQ(record.error,
              LzssHashTreePromotionError::invalid_reuse_counts);
    EXPECT_EQ(short_counts, (std::array<std::uint8_t, 3>{1, 2, 3}));
    EXPECT_FALSE(state.state_valid());
}

TEST(LzssHashTreePromotion, AdvanceBeginsAndMatchingCommitCompletes) {
    auto idle = make_state(10);
    const auto idle_begin = idle.begin_advance();
    EXPECT_FALSE(idle_begin.required);
    EXPECT_EQ(idle_begin.bucket, lzss_hash_tree_no_promotion_bucket);
    EXPECT_EQ(idle.phase(), LzssHashTreePromotionPhase::idle);

    auto state = make_state(10);
    ASSERT_TRUE(state.record_completed_chain_query(5, 12).pending);
    const auto begin = state.begin_advance();
    ASSERT_EQ(begin.error, LzssHashTreePromotionError::none);
    EXPECT_TRUE(begin.required);
    EXPECT_EQ(begin.bucket, 5U);
    EXPECT_EQ(begin.trigger_candidate_count, 12U);
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::building);
    EXPECT_EQ(state.commit(5), LzssHashTreePromotionError::none);
    EXPECT_EQ(state.phase(), LzssHashTreePromotionPhase::idle);
    EXPECT_EQ(state.active_bucket(), lzss_hash_tree_no_promotion_bucket);
    EXPECT_EQ(state.trigger_candidate_count(), 0U);
    EXPECT_TRUE(state.state_valid());
}

TEST(LzssHashTreePromotion, InvalidBucketIsSticky) {
    auto state = make_state(10);
    const auto invalid = state.record_completed_chain_query(8, 11);
    EXPECT_EQ(invalid.error, LzssHashTreePromotionError::invalid_bucket);
    EXPECT_FALSE(state.state_valid());
    EXPECT_EQ(state.last_error(), LzssHashTreePromotionError::invalid_bucket);
    EXPECT_FALSE(state.record_completed_chain_query(1, 11).pending);
    EXPECT_EQ(state.begin_advance().error,
              LzssHashTreePromotionError::invalid_bucket);
    EXPECT_EQ(state.commit(1), LzssHashTreePromotionError::invalid_bucket);
}

TEST(LzssHashTreePromotion, InvalidTransitionsPreserveFirstError) {
    auto pending = make_state(10);
    ASSERT_TRUE(pending.record_completed_chain_query(2, 11).pending);
    EXPECT_EQ(pending.record_completed_chain_query(3, 11).error,
              LzssHashTreePromotionError::invalid_transition);
    EXPECT_FALSE(pending.state_valid());
    EXPECT_EQ(pending.last_error(),
              LzssHashTreePromotionError::invalid_transition);

    auto building = make_state(10);
    ASSERT_TRUE(building.record_completed_chain_query(2, 11).pending);
    ASSERT_TRUE(building.begin_advance().required);
    EXPECT_EQ(building.record_completed_chain_query(2, 11).error,
              LzssHashTreePromotionError::invalid_transition);
    EXPECT_EQ(building.commit(2),
              LzssHashTreePromotionError::invalid_transition);

    auto wrong_commit = make_state(10);
    ASSERT_TRUE(wrong_commit.record_completed_chain_query(2, 11).pending);
    ASSERT_TRUE(wrong_commit.begin_advance().required);
    EXPECT_EQ(wrong_commit.commit(3),
              LzssHashTreePromotionError::invalid_transition);
    EXPECT_FALSE(wrong_commit.state_valid());

    auto repeated_begin = make_state(10);
    ASSERT_TRUE(repeated_begin.record_completed_chain_query(2, 11).pending);
    ASSERT_TRUE(repeated_begin.begin_advance().required);
    EXPECT_EQ(repeated_begin.begin_advance().error,
              LzssHashTreePromotionError::invalid_transition);
    EXPECT_FALSE(repeated_begin.state_valid());
}

TEST(LzssHashTreePromotion, UninitializedUseIsStickyInvalid) {
    LzssHashTreePromotionState state{};
    EXPECT_EQ(state.record_completed_chain_query(0, 1).error,
              LzssHashTreePromotionError::invalid_transition);
    EXPECT_FALSE(state.state_valid());
    EXPECT_EQ(state.last_error(),
              LzssHashTreePromotionError::invalid_transition);
}

} // namespace

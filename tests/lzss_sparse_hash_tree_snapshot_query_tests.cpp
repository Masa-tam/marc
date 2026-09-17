#include "dictionary/lzss_sparse_hash_tree_snapshot_query.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
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

struct SnapshotFixture {
    std::vector<std::byte> input{bytes(
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")};
    LzssParameters parameters{};
    std::array<std::uint32_t, 2> left{
        1, lzss_hash_tree_null_node};
    std::array<std::uint32_t, 2> right{
        lzss_hash_tree_null_node, lzss_hash_tree_null_node};
    std::array<std::uint32_t, 2> parent{
        lzss_hash_tree_null_node, 0};
    std::array<std::uint8_t, 2> height{2, 1};
    std::array<LzssHashTreeStoredPosition, 2> position{10, 5};
    std::array<LzssHashTreeStoredPosition, 2> subtree_maximum{10, 5};
    std::array<std::uint32_t, 20> links{};

    SnapshotFixture() {
        parameters.window_size = 20;
        parameters.max_match_length = 5;
    }

    [[nodiscard]] LzssSparseHashTreeSnapshotQueryContext context(
        const std::size_t query_position,
        const LzssHashTreeStoredPosition head) const noexcept {
        return {{input, parameters, query_position, 0, 1, 0,
                 left, right, parent, height, position, subtree_maximum,
                 nullptr, LzssHashTreeNodeIdentity::pool_local},
                head, links};
    }
};

TEST(LzssSparseHashTreeSnapshotQuery,
     UsesSnapshotWhenDeltaBeginsAtWatermark) {
    const SnapshotFixture fixture{};
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(15, 10));
    ASSERT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::none);
    EXPECT_EQ(result.snapshot_watermark, 10U);
    EXPECT_EQ(result.delta_candidates_visited, 0U);
    EXPECT_EQ(result.candidate_position, 10U);
    EXPECT_EQ(result.match, (LzssMatch{5, 5}));
}

TEST(LzssSparseHashTreeSnapshotQuery,
     NewestEqualDeltaCandidateWinsAtWatermarkCutoff) {
    SnapshotFixture fixture{};
    fixture.links[14U % fixture.links.size()] = 2;
    fixture.links[12U % fixture.links.size()] = 2;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(15, 14));
    ASSERT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::none);
    EXPECT_EQ(result.delta_candidates_visited, 1U);
    EXPECT_EQ(result.candidate_position, 14U);
    EXPECT_EQ(result.match, (LzssMatch{1, 5}));
}

TEST(LzssSparseHashTreeSnapshotQuery,
     LongerSnapshotBeatsNewerDeltaCandidate) {
    SnapshotFixture fixture{};
    fixture.parameters.max_match_length = 6;
    fixture.input[19] = std::byte{'B'};
    fixture.links[14U % fixture.links.size()] = 4;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(30, 14));
    ASSERT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::none);
    EXPECT_EQ(result.delta_candidates_visited, 1U);
    EXPECT_EQ(result.candidate_position, 10U);
    EXPECT_EQ(result.match, (LzssMatch{20, 6}));
}

TEST(LzssSparseHashTreeSnapshotQuery,
     LongerDeltaCandidateBeatsSnapshot) {
    SnapshotFixture fixture{};
    fixture.parameters.max_match_length = 6;
    fixture.input[15] = std::byte{'B'};
    fixture.links[20U % fixture.links.size()] = 10;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(30, 20));
    ASSERT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::none);
    EXPECT_EQ(result.delta_candidates_visited, 1U);
    EXPECT_EQ(result.candidate_position, 20U);
    EXPECT_EQ(result.match, (LzssMatch{10, 6}));
}

TEST(LzssSparseHashTreeSnapshotQuery,
     ExpiredSnapshotFallsBackToActiveDelta) {
    SnapshotFixture fixture{};
    fixture.parameters.window_size = 5;
    std::array<std::uint32_t, 5> links{};
    links[19U % links.size()] = 1;
    links[18U % links.size()] = 8;
    auto context = fixture.context(19, 18);
    context.links = links;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        context);
    ASSERT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::none);
    EXPECT_EQ(result.snapshot_nodes_visited, 1U);
    EXPECT_EQ(result.delta_candidates_visited, 1U);
    EXPECT_EQ(result.candidate_position, 18U);
    EXPECT_EQ(result.match, (LzssMatch{1, 5}));
}

TEST(LzssSparseHashTreeSnapshotQuery,
     RejectsInvalidDeltaWithoutMutatingInputs) {
    SnapshotFixture fixture{};
    fixture.parameters.max_match_length = 6;
    fixture.input[19] = std::byte{'B'};
    fixture.links[14U % fixture.links.size()] = 15;
    const auto before = fixture.links;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(30, 14));
    EXPECT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::invalid_delta);
    EXPECT_EQ(fixture.links, before);
}

TEST(LzssSparseHashTreeSnapshotQuery,
     RejectsDeltaThatSkipsActiveWatermark) {
    SnapshotFixture fixture{};
    fixture.parameters.max_match_length = 6;
    fixture.input[19] = std::byte{'B'};
    fixture.links[14U % fixture.links.size()] = 5;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(30, 14));
    EXPECT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::invalid_delta);
    EXPECT_EQ(result.delta_candidates_visited, 1U);
}

TEST(LzssSparseHashTreeSnapshotQuery,
     PropagatesSnapshotFailureBeforeDeltaTraversal) {
    SnapshotFixture fixture{};
    fixture.subtree_maximum[0] = 9;
    const auto result = query_lzss_sparse_hash_tree_snapshot_delta_exact(
        fixture.context(15, 14));
    EXPECT_EQ(result.error,
              LzssSparseHashTreeSnapshotQueryError::snapshot_failure);
    EXPECT_EQ(result.snapshot_error,
              LzssHashTreeBucketQueryError::invalid_root);
    EXPECT_EQ(result.delta_candidates_visited, 0U);
}

} // namespace
} // namespace marc::dictionary::internal

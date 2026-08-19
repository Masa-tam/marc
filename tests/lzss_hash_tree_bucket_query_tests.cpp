#include "dictionary/lzss_hash_tree_bucket_query.hpp"

#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_hash_tree_bucket_builder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

struct QueryFixture {
    QueryFixture(
        std::vector<std::byte> values, const std::size_t query,
        const std::uint32_t window = 16)
        : input(std::move(values)), query_position(query) {
        parameters.window_size = window;
        parameters.max_match_length = 5;
        const auto capacity = std::min<std::size_t>(input.size(), window);
        links.assign(capacity, 0);
        left.assign(capacity, lzss_hash_tree_null_node);
        right.assign(capacity, lzss_hash_tree_null_node);
        parent.assign(capacity, lzss_hash_tree_null_node);
        height.assign(capacity, 0);
        position.assign(capacity, lzss_hash_tree_no_stored_position);
        subtree_maximum.assign(
            capacity, lzss_hash_tree_no_stored_position);

        const auto lower = query_position > parameters.window_size
            ? query_position - parameters.window_size : 0U;
        auto previous = lzss_hash_tree_no_position;
        for (auto candidate = query_position; candidate-- > lower;) {
            if (input.size() - candidate < lzss_match_finder_prefix_size) {
                continue;
            }
            if (head == lzss_hash_tree_no_position) head = candidate;
            if (previous != lzss_hash_tree_no_position) {
                links[previous % capacity] = static_cast<std::uint32_t>(
                    previous - candidate);
            }
            previous = candidate;
        }
        if (previous != lzss_hash_tree_no_position) {
            links[previous % capacity] = 0;
        }
    }

    [[nodiscard]] LzssHashTreeBucketBuildContext build_context() {
        return {
            input, parameters, query_position, 0, 1, head, links,
            {left, right, parent, height, position, subtree_maximum}};
    }

    [[nodiscard]] LzssHashTreeBucketQueryContext query_context(
        const std::uint32_t root,
        LzssHashTreeComponentStatistics* const statistics = nullptr) const {
        return {
            input, parameters, query_position, 0, 1, root,
            left, right, parent, height, position, subtree_maximum,
            statistics};
    }

    std::vector<std::byte> input{};
    std::size_t query_position{};
    LzssParameters parameters{};
    std::size_t head{lzss_hash_tree_no_position};
    std::vector<std::uint32_t> links{};
    std::vector<std::uint32_t> left{};
    std::vector<std::uint32_t> right{};
    std::vector<std::uint32_t> parent{};
    std::vector<std::uint8_t> height{};
    std::vector<LzssHashTreeStoredPosition> position{};
    std::vector<LzssHashTreeStoredPosition> subtree_maximum{};
};

void expect_exact_query(
    const std::vector<std::byte>& input, const std::size_t query_position,
    const std::uint32_t window = 16) {
    QueryFixture fixture{input, query_position, window};
    const auto build = build_lzss_hash_tree_bucket(fixture.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);

    LzssExhaustiveMatchFinder exhaustive{input, fixture.parameters};
    exhaustive.advance(0, query_position);
    const auto expected = exhaustive.find_match(query_position);

    const auto required = calculate_lzss_hash_chain_workspace(
        input.size(), fixture.parameters, {});
    ASSERT_EQ(required.error, LzssHashChainError::none);
    auto storage = make_storage(required.workspace_size);
    LzssHashChainMatchFinder chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, fixture.parameters, {}, storage.bytes, chain),
              LzssHashChainError::none);
    chain.advance(0, query_position);
    ASSERT_EQ(chain.find_match(query_position), expected);

    const auto original_left = fixture.left;
    const auto original_right = fixture.right;
    const auto original_parent = fixture.parent;
    const auto original_height = fixture.height;
    const auto original_position = fixture.position;
    const auto original_maximum = fixture.subtree_maximum;
    const auto result = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(build.root));
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.match, expected);
    if (expected.length == 0) {
        EXPECT_EQ(result.candidate_position, lzss_hash_tree_no_position);
    } else {
        EXPECT_EQ(result.candidate_position,
                  query_position - expected.distance);
    }
    EXPECT_EQ(fixture.left, original_left);
    EXPECT_EQ(fixture.right, original_right);
    EXPECT_EQ(fixture.parent, original_parent);
    EXPECT_EQ(fixture.height, original_height);
    EXPECT_EQ(fixture.position, original_position);
    EXPECT_EQ(fixture.subtree_maximum, original_maximum);
}

TEST(LzssHashTreeBucketQuery, MatchesExactOraclesAcrossInputFamilies) {
    const auto repetitive = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto periodic = bytes("ABCDEABCDEABCDEABCDEABCDEABCDE");
    const auto distinct = bytes("0123456789abcdefghijklmnopqrstuv");
    const auto collision = std::vector<std::byte>{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}, std::byte{0}, std::byte{0x20}, std::byte{0},
        std::byte{0x58}, std::byte{0x59}, std::byte{1}, std::byte{0},
        std::byte{0}, std::byte{0x58}, std::byte{0x59}, std::byte{1},
        std::byte{0}, std::byte{0}, std::byte{0x58}, std::byte{0x59}};
    for (const auto* input : {&repetitive, &periodic, &distinct, &collision}) {
        for (std::size_t query = 5; query + 5 <= input->size(); ++query) {
            expect_exact_query(*input, query);
        }
    }
}

TEST(LzssHashTreeBucketQuery, SelectsNearestEqualCappedCandidate) {
    QueryFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    const auto build = build_lzss_hash_tree_bucket(fixture.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);

    const auto result = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(build.root));
    ASSERT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.maximum_lcp, 5U);
    EXPECT_EQ(result.candidate_position, 14U);
    EXPECT_EQ(result.match, (LzssMatch{1, 5}));
}

TEST(LzssHashTreeBucketQuery, SupportsPoolLocalNodeIdentity) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAA");
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;

    constexpr std::size_t capacity = 7;
    std::vector<std::uint32_t> left(
        capacity, lzss_hash_tree_null_node);
    std::vector<std::uint32_t> right(
        capacity, lzss_hash_tree_null_node);
    std::vector<std::uint32_t> parent(
        capacity, lzss_hash_tree_null_node);
    std::vector<std::uint8_t> height(capacity, 0);
    std::vector<LzssHashTreeStoredPosition> position(
        capacity, lzss_hash_tree_no_stored_position);
    std::vector<LzssHashTreeStoredPosition> subtree_maximum(
        capacity, lzss_hash_tree_no_stored_position);

    constexpr std::uint32_t root = 1;
    constexpr std::uint32_t older = 4;
    constexpr std::uint32_t newer = 6;
    left[root] = older;
    right[root] = newer;
    parent[older] = root;
    parent[newer] = root;
    height[root] = 2;
    height[older] = 1;
    height[newer] = 1;
    position[root] = 5;
    position[older] = 0;
    position[newer] = 10;
    subtree_maximum[root] = 10;
    subtree_maximum[older] = 0;
    subtree_maximum[newer] = 10;

    LzssHashTreeBucketQueryContext context{
        input, parameters, 15, 0, 1, root,
        left, right, parent, height, position, subtree_maximum,
        nullptr, LzssHashTreeNodeIdentity::pool_local};
    const auto result = query_lzss_hash_tree_bucket_exact(context);
    ASSERT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.candidate_position, 10U);
    EXPECT_EQ(result.match, (LzssMatch{5, 5}));

    context.node_identity = LzssHashTreeNodeIdentity::ring_position;
    EXPECT_EQ(query_lzss_hash_tree_bucket_exact(context).error,
              LzssHashTreeBucketQueryError::invalid_node_arrays);
}

TEST(LzssHashTreeBucketQuery, PoolLocalCandidateLookupRejectsMissingPosition) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAA");
    LzssParameters parameters{};
    parameters.window_size = 20;
    parameters.max_match_length = 5;

    std::array<std::uint32_t, 4> left{
        lzss_hash_tree_null_node, 0, 3, lzss_hash_tree_null_node};
    std::array<std::uint32_t, 4> right{
        lzss_hash_tree_null_node, 2, lzss_hash_tree_null_node,
        lzss_hash_tree_null_node};
    std::array<std::uint32_t, 4> parent{
        1, lzss_hash_tree_null_node, 1, 2};
    std::array<std::uint8_t, 4> height{1, 3, 2, 1};
    std::array<LzssHashTreeStoredPosition, 4> position{0, 5, 9, 7};
    std::array<LzssHashTreeStoredPosition, 4> subtree_maximum{0, 10, 10, 10};

    const LzssHashTreeBucketQueryContext context{
        input, parameters, 15, 0, 1, 1,
        left, right, parent, height, position, subtree_maximum,
        nullptr, LzssHashTreeNodeIdentity::pool_local};
    const auto result = query_lzss_hash_tree_bucket_exact(context);
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::invalid_tree);
    EXPECT_EQ(result.match, LzssMatch{});

    subtree_maximum[3] = lzss_hash_tree_no_stored_position;
    const auto out_of_range = query_lzss_hash_tree_bucket_exact(context);
    EXPECT_EQ(out_of_range.error,
              LzssHashTreeBucketQueryError::invalid_tree);
    EXPECT_EQ(out_of_range.match, LzssMatch{});
}

TEST(LzssHashTreeBucketQuery, EmptyRootAndShortQueryAreEmpty) {
    QueryFixture empty{bytes("0123456789ABCDEFGHIJ"), 10};
    auto result = query_lzss_hash_tree_bucket_exact(
        empty.query_context(lzss_hash_tree_null_node));
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.match, LzssMatch{});

    QueryFixture short_query{bytes("0123456789ABCDEFGHIJ"), 18};
    result = query_lzss_hash_tree_bucket_exact(
        short_query.query_context(lzss_hash_tree_null_node));
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.match, LzssMatch{});
}

TEST(LzssHashTreeBucketQuery, RejectsInvalidContextAndReachedNode) {
    QueryFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    const auto build = build_lzss_hash_tree_bucket(fixture.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);

    auto context = fixture.query_context(build.root);
    context.bucket_count = 3;
    EXPECT_EQ(query_lzss_hash_tree_bucket_exact(context).error,
              LzssHashTreeBucketQueryError::invalid_bucket);

    context = fixture.query_context(build.root);
    context.query_position = fixture.input.size() + 1U;
    EXPECT_EQ(query_lzss_hash_tree_bucket_exact(context).error,
              LzssHashTreeBucketQueryError::invalid_query_position);

    context = fixture.query_context(
        static_cast<std::uint32_t>(fixture.left.size()));
    EXPECT_EQ(query_lzss_hash_tree_bucket_exact(context).error,
              LzssHashTreeBucketQueryError::invalid_root);

    fixture.position[build.root] = fixture.query_position;
    context = fixture.query_context(build.root);
    EXPECT_EQ(query_lzss_hash_tree_bucket_exact(context).error,
              LzssHashTreeBucketQueryError::invalid_root);
}

TEST(LzssHashTreeBucketQuery, CycleAndBadSubtreeMaximumFailFinitely) {
    QueryFixture cycle{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    auto build = build_lzss_hash_tree_bucket(cycle.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);
    cycle.right[build.root] = build.root;
    auto result = query_lzss_hash_tree_bucket_exact(
        cycle.query_context(build.root));
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::invalid_tree);
    EXPECT_LE(result.nodes_visited, cycle.left.size());

    QueryFixture maximum{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    build = build_lzss_hash_tree_bucket(maximum.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);
    const auto right = maximum.right[build.root];
    ASSERT_NE(right, lzss_hash_tree_null_node);
    const auto left_of_right = maximum.left[right];
    ASSERT_NE(left_of_right, lzss_hash_tree_null_node);
    maximum.subtree_maximum[left_of_right] = maximum.query_position;
    result = query_lzss_hash_tree_bucket_exact(
        maximum.query_context(build.root));
    EXPECT_EQ(result.error, LzssHashTreeBucketQueryError::invalid_tree);
    EXPECT_EQ(result.match, LzssMatch{});
}

TEST(LzssHashTreeBucketQuery, ReportsSeparatedComparisonWork) {
    QueryFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    const auto build = build_lzss_hash_tree_bucket(fixture.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);
    const auto plain = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(build.root));
    LzssHashTreeComponentStatistics statistics{};
    const auto result = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(build.root, &statistics));
    ASSERT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_EQ(result.match, plain.match);
    EXPECT_EQ(result.candidate_position, plain.candidate_position);
    EXPECT_EQ(result.maximum_lcp, plain.maximum_lcp);
    EXPECT_EQ(result.nodes_visited, plain.nodes_visited);
    EXPECT_EQ(result.match, (LzssMatch{1, 5}));
    EXPECT_GT(statistics.key_comparison_count, 0U);
    EXPECT_GT(statistics.key_byte_comparison_count, 0U);
    EXPECT_GT(statistics.lcp_byte_comparison_count, 0U);
    EXPECT_GT(statistics.prefix_range_comparison_count, 0U);
    EXPECT_GT(statistics.prefix_range_byte_comparison_count, 0U);
    EXPECT_EQ(statistics.lcp_skipped_byte_count, 0U);
}

TEST(LzssHashTreeBucketQuery, StatisticsSaturate) {
    QueryFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 15, 20};
    const auto build = build_lzss_hash_tree_bucket(fixture.build_context());
    ASSERT_EQ(build.error, LzssHashTreeBucketBuildError::none);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    LzssHashTreeComponentStatistics statistics{};
    statistics.key_comparison_count = maximum;
    statistics.key_byte_comparison_count = maximum;
    statistics.lcp_byte_comparison_count = maximum;
    statistics.prefix_range_comparison_count = maximum;
    statistics.prefix_range_byte_comparison_count = maximum;
    const auto result = query_lzss_hash_tree_bucket_exact(
        fixture.query_context(build.root, &statistics));
    ASSERT_EQ(result.error, LzssHashTreeBucketQueryError::none);
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.key_comparison_count, maximum);
    EXPECT_EQ(statistics.key_byte_comparison_count, maximum);
    EXPECT_EQ(statistics.lcp_byte_comparison_count, maximum);
    EXPECT_EQ(statistics.prefix_range_comparison_count, maximum);
    EXPECT_EQ(statistics.prefix_range_byte_comparison_count, maximum);
}

} // namespace

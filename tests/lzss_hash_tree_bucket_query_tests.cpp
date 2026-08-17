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
        position.assign(capacity, std::numeric_limits<std::size_t>::max());
        subtree_maximum.assign(
            capacity, std::numeric_limits<std::size_t>::max());

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
        const std::uint32_t root) const {
        return {
            input, parameters, query_position, 0, 1, root,
            left, right, parent, height, position, subtree_maximum};
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
    std::vector<std::size_t> position{};
    std::vector<std::size_t> subtree_maximum{};
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

} // namespace

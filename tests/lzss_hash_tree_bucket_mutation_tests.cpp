#include "dictionary/lzss_hash_tree_bucket_mutation.hpp"

#include "dictionary/lzss_hash_tree_bucket_query.hpp"
#include "dictionary/lzss_prefix_hash.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

struct MutationFixture {
    explicit MutationFixture(
        std::vector<std::byte> values,
        const std::uint32_t window = 16)
        : input(std::move(values)) {
        parameters.window_size = window;
        parameters.max_match_length = 5;
        const auto capacity = std::min<std::size_t>(input.size(), window);
        left.assign(capacity, lzss_hash_tree_null_node);
        right.assign(capacity, lzss_hash_tree_null_node);
        parent.assign(capacity, lzss_hash_tree_null_node);
        height.assign(capacity, 0);
        position.assign(capacity, lzss_hash_tree_no_position);
        subtree_maximum.assign(capacity, lzss_hash_tree_no_position);
    }

    [[nodiscard]] LzssHashTreeBucketMutationContext context(
        LzssHashTreeComponentStatistics* const statistics = nullptr) {
        return {
            input, parameters, 0, 1, left, right, parent, height,
            position, subtree_maximum, statistics};
    }

    [[nodiscard]] LzssHashTreeBucketQueryContext query_context(
        const std::size_t query_position) const {
        return {
            input, parameters, query_position, 0, 1, root,
            left, right, parent, height, position, subtree_maximum};
    }

    void insert(const std::size_t absolute_position) {
        const auto result = insert_lzss_hash_tree_bucket_position(
            context(), root, absolute_position);
        ASSERT_EQ(result.error, LzssHashTreeBucketMutationError::none);
        root = result.root;
    }

    void remove(const std::size_t absolute_position) {
        const auto result = remove_lzss_hash_tree_bucket_position(
            context(), root, absolute_position);
        ASSERT_EQ(result.error, LzssHashTreeBucketMutationError::none);
        root = result.root;
    }

    void expect_valid(
        const std::size_t begin, const std::size_t end) {
        EXPECT_EQ(validate_lzss_hash_tree_bucket_active_range(
                      context(), root, begin, end),
                  LzssHashTreeBucketMutationError::none);
    }

    std::vector<std::byte> input{};
    LzssParameters parameters{};
    std::uint32_t root{lzss_hash_tree_null_node};
    std::vector<std::uint32_t> left{};
    std::vector<std::uint32_t> right{};
    std::vector<std::uint32_t> parent{};
    std::vector<std::uint8_t> height{};
    std::vector<std::size_t> position{};
    std::vector<std::size_t> subtree_maximum{};
};

TEST(LzssHashTreeBucketMutation, InsertsAndSlidesAcrossRingReuse) {
    MutationFixture fixture{
        bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 8};
    LzssExhaustiveMatchFinder exhaustive{fixture.input, fixture.parameters};

    for (std::size_t current = 0; current + 5 <= fixture.input.size();
         ++current) {
        if (current >= fixture.parameters.window_size) {
            fixture.remove(current - fixture.parameters.window_size);
        }
        fixture.insert(current);
        const auto begin = current + 1U > fixture.parameters.window_size
            ? current + 1U - fixture.parameters.window_size : 0U;
        fixture.expect_valid(begin, current + 1U);

        exhaustive.advance(current, current + 1U);
        if (current + 1U + 5U <= fixture.input.size()) {
            const auto expected = exhaustive.find_match(current + 1U);
            const auto actual = query_lzss_hash_tree_bucket_exact(
                fixture.query_context(current + 1U));
            ASSERT_EQ(actual.error, LzssHashTreeBucketQueryError::none);
            EXPECT_EQ(actual.match, expected) << current;
        }
    }
}

TEST(LzssHashTreeBucketMutation, RemovesStructuralShapesAndRestoresSet) {
    MutationFixture fixture{
        bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 16};
    for (std::size_t position = 0; position < 15; ++position) {
        fixture.insert(position);
    }
    fixture.expect_valid(0, 15);

    std::vector<std::size_t> targets{};
    targets.push_back(fixture.position[fixture.root]);
    bool found_leaf{};
    bool found_one_child{};
    bool found_direct_successor{};
    bool found_deep_successor{};
    for (std::size_t node = 0; node < 15; ++node) {
        const auto child_count =
            (fixture.left[node] != lzss_hash_tree_null_node ? 1U : 0U)
            + (fixture.right[node] != lzss_hash_tree_null_node ? 1U : 0U);
        if (!found_leaf && child_count == 0) {
            targets.push_back(fixture.position[node]);
            found_leaf = true;
        }
        if (!found_one_child && child_count == 1) {
            targets.push_back(fixture.position[node]);
            found_one_child = true;
        }
        if (child_count == 2) {
            auto successor = fixture.right[node];
            while (fixture.left[successor] != lzss_hash_tree_null_node) {
                successor = fixture.left[successor];
            }
            const auto direct = fixture.parent[successor] == node;
            if (direct && !found_direct_successor) {
                targets.push_back(fixture.position[node]);
                found_direct_successor = true;
            }
            if (!direct && !found_deep_successor) {
                targets.push_back(fixture.position[node]);
                found_deep_successor = true;
            }
        }
    }
    EXPECT_TRUE(found_leaf);
    EXPECT_TRUE(found_direct_successor);
    EXPECT_TRUE(found_deep_successor);

    for (const auto target : targets) {
        MutationFixture candidate{
            bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 16};
        for (std::size_t position = 0; position < 15; ++position) {
            candidate.insert(position);
        }
        candidate.remove(target);
        EXPECT_EQ(candidate.position[target % candidate.position.size()],
                  lzss_hash_tree_no_position);
        candidate.insert(target);
        candidate.expect_valid(0, 15);
    }

    MutationFixture one_child{
        bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 16};
    for (std::size_t position = 0; position < 6; ++position) {
        one_child.insert(position);
    }
    for (std::size_t node = 0; node < 6; ++node) {
        const auto child_count =
            (one_child.left[node] != lzss_hash_tree_null_node ? 1U : 0U)
            + (one_child.right[node] != lzss_hash_tree_null_node ? 1U : 0U);
        if (child_count != 1) continue;
        const auto target = one_child.position[node];
        one_child.remove(target);
        one_child.insert(target);
        one_child.expect_valid(0, 6);
        found_one_child = true;
        break;
    }
    EXPECT_TRUE(found_one_child);
}

TEST(LzssHashTreeBucketMutation, EmptyPromotedBucketCanBecomeActiveAgain) {
    MutationFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 8};
    fixture.insert(0);
    fixture.remove(0);
    EXPECT_EQ(fixture.root, lzss_hash_tree_null_node);
    fixture.expect_valid(1, 1);
    fixture.insert(1);
    fixture.expect_valid(1, 2);
}

TEST(LzssHashTreeBucketMutation, PreflightFailuresPreserveTreeBytes) {
    MutationFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 16};
    for (std::size_t position = 0; position < 8; ++position) {
        fixture.insert(position);
    }
    const auto root = fixture.root;
    const auto left = fixture.left;
    const auto right = fixture.right;
    const auto parent = fixture.parent;
    const auto height = fixture.height;
    const auto positions = fixture.position;
    const auto maxima = fixture.subtree_maximum;
    const auto expect_unchanged = [&]() {
        EXPECT_EQ(fixture.root, root);
        EXPECT_EQ(fixture.left, left);
        EXPECT_EQ(fixture.right, right);
        EXPECT_EQ(fixture.parent, parent);
        EXPECT_EQ(fixture.height, height);
        EXPECT_EQ(fixture.position, positions);
        EXPECT_EQ(fixture.subtree_maximum, maxima);
    };

    auto result = insert_lzss_hash_tree_bucket_position(
        fixture.context(), root, 7);
    EXPECT_EQ(result.error,
              LzssHashTreeBucketMutationError::duplicate_position);
    expect_unchanged();

    result = remove_lzss_hash_tree_bucket_position(
        fixture.context(), root, 9);
    EXPECT_EQ(result.error,
              LzssHashTreeBucketMutationError::missing_position);
    expect_unchanged();

    result = insert_lzss_hash_tree_bucket_position(
        fixture.context(), static_cast<std::uint32_t>(fixture.left.size()), 8);
    EXPECT_EQ(result.error, LzssHashTreeBucketMutationError::invalid_root);
    expect_unchanged();

    result = insert_lzss_hash_tree_bucket_position(
        fixture.context(), root, fixture.input.size() - 3U);
    EXPECT_EQ(result.error,
              LzssHashTreeBucketMutationError::invalid_position);
    expect_unchanged();

    auto wrong_bucket = fixture.context();
    wrong_bucket.bucket_count = 2;
    const auto hash = calculate_lzss_prefix_hash(fixture.input, 8);
    ASSERT_TRUE(hash.valid);
    wrong_bucket.bucket =
        (static_cast<std::size_t>(hash.value) & 1U) ^ 1U;
    result = insert_lzss_hash_tree_bucket_position(
        wrong_bucket, root, 8);
    EXPECT_NE(result.error, LzssHashTreeBucketMutationError::none);
    expect_unchanged();
}

TEST(LzssHashTreeBucketMutation, SearchCycleFailsFinitelyBeforeMutation) {
    MutationFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 16};
    for (std::size_t position = 0; position < 8; ++position) {
        fixture.insert(position);
    }
    fixture.right[fixture.root] = fixture.root;
    const auto left = fixture.left;
    const auto right = fixture.right;
    const auto parent = fixture.parent;
    const auto height = fixture.height;
    const auto positions = fixture.position;
    const auto maxima = fixture.subtree_maximum;

    const auto result = insert_lzss_hash_tree_bucket_position(
        fixture.context(), fixture.root, 8);
    EXPECT_EQ(result.error, LzssHashTreeBucketMutationError::invalid_tree);
    EXPECT_EQ(fixture.left, left);
    EXPECT_EQ(fixture.right, right);
    EXPECT_EQ(fixture.parent, parent);
    EXPECT_EQ(fixture.height, height);
    EXPECT_EQ(fixture.position, positions);
    EXPECT_EQ(fixture.subtree_maximum, maxima);
}

TEST(LzssHashTreeBucketMutation, ReportsMaintenanceWork) {
    MutationFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 16};
    LzssHashTreeComponentStatistics statistics{};
    for (std::size_t position = 0; position < 3; ++position) {
        const auto result = insert_lzss_hash_tree_bucket_position(
            fixture.context(&statistics), fixture.root, position);
        ASSERT_EQ(result.error, LzssHashTreeBucketMutationError::none);
        fixture.root = result.root;
    }
    EXPECT_GT(statistics.key_comparison_count, 0U);
    EXPECT_GT(statistics.key_byte_comparison_count, 0U);
    EXPECT_EQ(statistics.rotation_count, 1U);
    EXPECT_EQ(statistics.maximum_height, 2U);
    fixture.expect_valid(0, 3);
}

TEST(LzssHashTreeBucketMutation, ObserverDoesNotChangeSlidingTree) {
    MutationFixture plain{bytes("AAAAAAAAAAAAAAAAAAAA"), 8};
    MutationFixture observed{bytes("AAAAAAAAAAAAAAAAAAAA"), 8};
    LzssHashTreeComponentStatistics statistics{};
    for (std::size_t position = 0; position < 10; ++position) {
        if (position >= 8) {
            plain.remove(position - 8);
            const auto removed = remove_lzss_hash_tree_bucket_position(
                observed.context(&statistics), observed.root, position - 8);
            ASSERT_EQ(removed.error, LzssHashTreeBucketMutationError::none);
            observed.root = removed.root;
        }
        plain.insert(position);
        const auto inserted = insert_lzss_hash_tree_bucket_position(
            observed.context(&statistics), observed.root, position);
        ASSERT_EQ(inserted.error, LzssHashTreeBucketMutationError::none);
        observed.root = inserted.root;
    }
    EXPECT_EQ(observed.root, plain.root);
    EXPECT_EQ(observed.left, plain.left);
    EXPECT_EQ(observed.right, plain.right);
    EXPECT_EQ(observed.parent, plain.parent);
    EXPECT_EQ(observed.height, plain.height);
    EXPECT_EQ(observed.position, plain.position);
    EXPECT_EQ(observed.subtree_maximum, plain.subtree_maximum);
    EXPECT_GT(statistics.key_comparison_count, 0U);
    EXPECT_GT(statistics.key_byte_comparison_count, 0U);
    EXPECT_GT(statistics.rotation_count, 0U);
    EXPECT_GT(statistics.maximum_height, 0U);
}

TEST(LzssHashTreeBucketMutation, StatisticsSaturate) {
    MutationFixture fixture{bytes("AAAAAAAAAAAAAAAAAAAA"), 16};
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    LzssHashTreeComponentStatistics statistics{};
    statistics.key_comparison_count = maximum;
    statistics.key_byte_comparison_count = maximum;
    statistics.rotation_count = maximum;
    for (std::size_t position = 0; position < 3; ++position) {
        const auto result = insert_lzss_hash_tree_bucket_position(
            fixture.context(&statistics), fixture.root, position);
        ASSERT_EQ(result.error, LzssHashTreeBucketMutationError::none);
        fixture.root = result.root;
    }
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.key_comparison_count, maximum);
    EXPECT_EQ(statistics.key_byte_comparison_count, maximum);
    EXPECT_EQ(statistics.rotation_count, maximum);
}

} // namespace

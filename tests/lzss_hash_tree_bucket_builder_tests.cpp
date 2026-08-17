#include "dictionary/lzss_hash_tree_bucket_builder.hpp"

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

struct BucketFixture {
    explicit BucketFixture(
        std::vector<std::byte> values,
        const std::uint32_t window_size = 20)
        : input(std::move(values)) {
        parameters.window_size = window_size;
        parameters.max_match_length = 5;
        const auto capacity = std::min<std::size_t>(
            input.size(), parameters.window_size);
        links.assign(capacity, 0);
        left.assign(capacity, 0xccccccccU);
        right.assign(capacity, 0xccccccccU);
        parent.assign(capacity, 0xccccccccU);
        height.assign(capacity, 0xccU);
        position.assign(
            capacity, std::numeric_limits<std::size_t>::max() - 1U);
        subtree_maximum.assign(
            capacity, std::numeric_limits<std::size_t>::max() - 1U);
    }

    [[nodiscard]] LzssHashTreeBucketBuildContext context(
        const std::size_t query_position,
        const std::size_t head_position,
        const std::size_t bucket = 0,
        const std::size_t bucket_count = 1,
        LzssHashTreeComponentStatistics* const statistics = nullptr) {
        return {
            input,
            parameters,
            query_position,
            bucket,
            bucket_count,
            head_position,
            links,
            {left, right, parent, height, position, subtree_maximum},
            statistics,
        };
    }

    std::vector<std::byte> input{};
    LzssParameters parameters{};
    std::vector<std::uint32_t> links{};
    std::vector<std::uint32_t> left{};
    std::vector<std::uint32_t> right{};
    std::vector<std::uint32_t> parent{};
    std::vector<std::uint8_t> height{};
    std::vector<std::size_t> position{};
    std::vector<std::size_t> subtree_maximum{};
};

void expect_three_node_build(
    const std::string_view input_text,
    const std::size_t expected_root_position) {
    BucketFixture fixture{bytes(input_text)};
    fixture.links[10] = 5;
    fixture.links[5] = 5;
    const auto context = fixture.context(15, 10);
    const auto original_links = fixture.links;

    const auto result = build_lzss_hash_tree_bucket(context);
    ASSERT_EQ(result.error, LzssHashTreeBucketBuildError::none);
    ASSERT_EQ(result.node_count, 3U);
    EXPECT_EQ(result.root, expected_root_position % fixture.links.size());
    EXPECT_EQ(fixture.parent[result.root], lzss_hash_tree_null_node);
    EXPECT_EQ(fixture.height[result.root], 2U);
    EXPECT_EQ(fixture.subtree_maximum[result.root], 10U);
    EXPECT_EQ(validate_lzss_hash_tree_bucket(
                  context, result.root, result.node_count),
              LzssHashTreeBucketBuildError::none);
    EXPECT_EQ(fixture.links, original_links);
    for (const auto absolute_position : {0U, 5U, 10U}) {
        EXPECT_EQ(fixture.position[absolute_position], absolute_position);
        auto node = static_cast<std::uint32_t>(absolute_position);
        for (std::size_t steps = 0; node != result.root; ++steps) {
            ASSERT_LT(steps, result.node_count);
            node = fixture.parent[node];
        }
    }
}

TEST(LzssHashTreeBucketBuilder, BuildsSingleAndDoubleRotationShapes) {
    expect_three_node_build("AAAAABBBBBCCCCCQQQQQ", 5);
    expect_three_node_build("CCCCCBBBBBAAAAAQQQQQ", 5);
    expect_three_node_build("BBBBBAAAAACCCCCQQQQQ", 0);
    expect_three_node_build("BBBBBCCCCCAAAAAQQQQQ", 0);
}

TEST(LzssHashTreeBucketBuilder, UsesAbsolutePositionForEqualCappedKeys) {
    expect_three_node_build("AAAAAAAAAAAAAAAAAAAA", 5);
}

TEST(LzssHashTreeBucketBuilder, IncludesWindowEdgeAndStopsBeyondIt) {
    BucketFixture fixture{bytes("012345678901234567890123456789"), 10};
    fixture.links[15 % fixture.links.size()] = 1;
    const auto context = fixture.context(25, 15);

    const auto result = build_lzss_hash_tree_bucket(context);
    ASSERT_EQ(result.error, LzssHashTreeBucketBuildError::none);
    EXPECT_EQ(result.node_count, 1U);
    EXPECT_EQ(result.root, 5U);
    EXPECT_EQ(fixture.position[5], 15U);
    EXPECT_EQ(fixture.height[5], 1U);
    EXPECT_EQ(fixture.subtree_maximum[5], 15U);
}

TEST(LzssHashTreeBucketBuilder, EmptyHeadDoesNotTouchNodeArrays) {
    BucketFixture fixture{bytes("AAAAABBBBBCCCCCQQQQQ")};
    const auto original_left = fixture.left;
    const auto original_right = fixture.right;
    const auto original_parent = fixture.parent;
    const auto original_height = fixture.height;
    const auto original_position = fixture.position;
    const auto original_maximum = fixture.subtree_maximum;

    const auto result = build_lzss_hash_tree_bucket(
        fixture.context(15, lzss_hash_tree_no_position));
    EXPECT_EQ(result.error, LzssHashTreeBucketBuildError::none);
    EXPECT_EQ(result.root, lzss_hash_tree_null_node);
    EXPECT_EQ(result.node_count, 0U);
    EXPECT_EQ(fixture.left, original_left);
    EXPECT_EQ(fixture.right, original_right);
    EXPECT_EQ(fixture.parent, original_parent);
    EXPECT_EQ(fixture.height, original_height);
    EXPECT_EQ(fixture.position, original_position);
    EXPECT_EQ(fixture.subtree_maximum, original_maximum);
}

TEST(LzssHashTreeBucketBuilder, RejectsInvalidContextAndChain) {
    BucketFixture fixture{bytes("AAAAABBBBBCCCCCQQQQQ")};
    fixture.links[10] = 5;
    fixture.links[5] = 5;

    auto context = fixture.context(15, 10);
    context.bucket_count = 3;
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_bucket);

    context = fixture.context(15, 10);
    context.nodes.height = context.nodes.height.first(
        context.nodes.height.size() - 1U);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_node_arrays);

    context = fixture.context(fixture.input.size() + 1U, 10);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_query_position);

    context = fixture.context(10, 10);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_head);

    context = fixture.context(fixture.input.size(), 17);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_head);

    fixture.links[10] = 11;
    context = fixture.context(15, 10);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::invalid_link);

    fixture.links[10] = 0;
    const auto hash = calculate_lzss_prefix_hash(fixture.input, 10);
    ASSERT_TRUE(hash.valid);
    const auto actual_bucket = static_cast<std::size_t>(hash.value) & 1U;
    context = fixture.context(15, 10, actual_bucket ^ 1U, 2);
    EXPECT_EQ(build_lzss_hash_tree_bucket(context).error,
              LzssHashTreeBucketBuildError::wrong_bucket);
}

TEST(LzssHashTreeBucketBuilder, ValidatorRejectsCorruptedMetadata) {
    BucketFixture fixture{bytes("AAAAABBBBBCCCCCQQQQQ")};
    fixture.links[10] = 5;
    fixture.links[5] = 5;
    const auto context = fixture.context(15, 10);
    const auto result = build_lzss_hash_tree_bucket(context);
    ASSERT_EQ(result.error, LzssHashTreeBucketBuildError::none);

    fixture.height[result.root] = 9;
    EXPECT_EQ(validate_lzss_hash_tree_bucket(
                  context, result.root, result.node_count),
              LzssHashTreeBucketBuildError::invalid_tree);
}

TEST(LzssHashTreeBucketBuilder, ReportsWorkWithoutChangingTree) {
    BucketFixture observed{bytes("BBBBBAAAAACCCCCQQQQQ")};
    BucketFixture plain{bytes("BBBBBAAAAACCCCCQQQQQ")};
    for (auto* fixture : {&observed, &plain}) {
        fixture->links[10] = 5;
        fixture->links[5] = 5;
    }
    LzssHashTreeComponentStatistics statistics{};
    const auto observed_result = build_lzss_hash_tree_bucket(
        observed.context(15, 10, 0, 1, &statistics));
    const auto plain_result = build_lzss_hash_tree_bucket(
        plain.context(15, 10));
    EXPECT_EQ(observed_result.root, plain_result.root);
    EXPECT_EQ(observed_result.node_count, plain_result.node_count);
    EXPECT_EQ(observed.left, plain.left);
    EXPECT_EQ(observed.right, plain.right);
    EXPECT_EQ(observed.parent, plain.parent);
    EXPECT_EQ(observed.height, plain.height);
    EXPECT_EQ(observed.position, plain.position);
    EXPECT_EQ(observed.subtree_maximum, plain.subtree_maximum);
    EXPECT_GT(statistics.key_comparison_count, 0U);
    EXPECT_GE(statistics.key_byte_comparison_count,
              statistics.key_comparison_count);
    EXPECT_EQ(statistics.rotation_count, 2U);
    EXPECT_EQ(statistics.maximum_height, 2U);
}

TEST(LzssHashTreeBucketBuilder, StatisticsSaturate) {
    BucketFixture fixture{bytes("AAAAABBBBBCCCCCQQQQQ")};
    fixture.links[10] = 5;
    fixture.links[5] = 5;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    LzssHashTreeComponentStatistics statistics{};
    statistics.key_comparison_count = maximum;
    statistics.key_byte_comparison_count = maximum;
    statistics.rotation_count = maximum;
    const auto result = build_lzss_hash_tree_bucket(
        fixture.context(15, 10, 0, 1, &statistics));
    ASSERT_EQ(result.error, LzssHashTreeBucketBuildError::none);
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.key_comparison_count, maximum);
    EXPECT_EQ(statistics.key_byte_comparison_count, maximum);
    EXPECT_EQ(statistics.rotation_count, maximum);
}

} // namespace

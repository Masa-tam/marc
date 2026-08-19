#include "dictionary/lzss_sparse_hash_tree_pool.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

struct AlignedStorage {
    std::vector<std::max_align_t> words{};
    std::span<std::byte> bytes{};
};

[[nodiscard]] AlignedStorage make_storage(const std::size_t byte_count) {
    AlignedStorage result{};
    const auto words = byte_count == 0 ? 0
        : (byte_count + sizeof(std::max_align_t) - 1U)
            / sizeof(std::max_align_t);
    result.words.resize(words);
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

struct Segment {
    std::size_t offset{};
    std::size_t count{};
    std::size_t element_size{};
    std::size_t alignment{};
};

TEST(LzssSparseHashTreePool, CalculatesEmptyAndSmallLayouts) {
    auto required = calculate_lzss_sparse_hash_tree_workspace(4, {}, {}, 0);
    EXPECT_EQ(required.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.bucket_count, 0U);
    EXPECT_EQ(required.chain_node_count, 0U);
    EXPECT_EQ(required.pool_node_capacity, 0U);

    required = calculate_lzss_sparse_hash_tree_workspace(5, {}, {}, 1);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(required.bucket_count, 8U);
    EXPECT_EQ(required.chain_node_count, 5U);
    EXPECT_EQ(required.pool_node_capacity, 1U);

    const std::array segments{
        Segment{required.head_offset, required.bucket_count,
                sizeof(LzssHashTreeStoredPosition),
                alignof(LzssHashTreeStoredPosition)},
        Segment{required.link_offset, required.chain_node_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.root_offset, required.bucket_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.mode_offset, required.bucket_count,
                sizeof(LzssSparseHashTreeBucketMode),
                alignof(LzssSparseHashTreeBucketMode)},
        Segment{required.bucket_node_count_offset, required.bucket_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.left_offset, required.pool_node_capacity,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.right_offset, required.pool_node_capacity,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.parent_offset, required.pool_node_capacity,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.height_offset, required.pool_node_capacity,
                sizeof(std::uint8_t), alignof(std::uint8_t)},
        Segment{required.position_offset, required.pool_node_capacity,
                sizeof(LzssHashTreeStoredPosition),
                alignof(LzssHashTreeStoredPosition)},
        Segment{required.subtree_maximum_position_offset,
                required.pool_node_capacity,
                sizeof(LzssHashTreeStoredPosition),
                alignof(LzssHashTreeStoredPosition)},
    };
    std::size_t previous_end{};
    for (const auto& segment : segments) {
        EXPECT_EQ(segment.offset % segment.alignment, 0U);
        EXPECT_GE(segment.offset, previous_end);
        previous_end = segment.offset + segment.count * segment.element_size;
    }
    EXPECT_EQ(previous_end, required.workspace_size);
}

TEST(LzssSparseHashTreePool, FixesFourMiBBaseAndPoolFormula) {
    constexpr std::size_t frame = 4U * 1024U * 1024U;
    marc::dictionary::internal::LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(frame);

    const auto no_pool = calculate_lzss_sparse_hash_tree_workspace(
        frame, parameters, {}, 0);
    ASSERT_EQ(no_pool.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(no_pool.bucket_count, 65'536U);
    EXPECT_EQ(no_pool.chain_node_count, frame);
    EXPECT_EQ(no_pool.workspace_size, 17'629'184U);

    const auto one = calculate_lzss_sparse_hash_tree_workspace(
        frame, parameters, {}, 1);
    ASSERT_EQ(one.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(one.workspace_size, no_pool.workspace_size + 24U);

    const auto four = calculate_lzss_sparse_hash_tree_workspace(
        frame, parameters, {}, 4);
    ASSERT_EQ(four.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(four.workspace_size, no_pool.workspace_size + 84U);

    const auto full = calculate_lzss_sparse_hash_tree_workspace(
        frame, parameters, {}, frame);
    ASSERT_EQ(full.error, LzssSparseHashTreeError::none);
    EXPECT_EQ(full.workspace_size,
              no_pool.workspace_size + 21U * frame);
}

TEST(LzssSparseHashTreePool, RejectsInvalidCapacityLimitsAndExtent) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_sparse_hash_tree_workspace(
                  5, {}, limits, 0).error,
              LzssSparseHashTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 0;
    EXPECT_EQ(calculate_lzss_sparse_hash_tree_workspace(
                  5, parameters, {}, 0).error,
              LzssSparseHashTreeError::invalid_parameters);

    auto invalid_capacity = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, {}, 6);
    EXPECT_EQ(invalid_capacity.error,
              LzssSparseHashTreeError::invalid_pool_capacity);

    limits = {};
    limits.max_internal_buffered_bytes = 64;
    limits.max_block_size = 64;
    const auto limited = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, limits, 1);
    EXPECT_EQ(limited.error,
              LzssSparseHashTreeError::workspace_limit_exceeded);

    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
        limits = {};
        limits.max_frame_size = UINT64_MAX;
        limits.max_total_output_size = UINT64_MAX;
        const auto unrepresentable = calculate_lzss_sparse_hash_tree_workspace(
            static_cast<std::size_t>(UINT32_MAX) + 1U, {}, limits, 0);
        EXPECT_EQ(unrepresentable.error,
                  LzssSparseHashTreeError::arithmetic_overflow);
    }

    limits = {};
    limits.max_frame_size = 4;
    const auto input_limited = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, limits, 0);
    EXPECT_EQ(input_limited.error,
              LzssSparseHashTreeError::input_limit_exceeded);
}

TEST(LzssSparseHashTreePool, AcceptsExactAggregateLimitAndRejectsOneLess) {
    const auto baseline = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, {}, 1);
    ASSERT_EQ(baseline.error, LzssSparseHashTreeError::none);
    const auto exact_limit = static_cast<std::uint64_t>(
        5U + baseline.workspace_size);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = exact_limit;
    limits.max_block_size = exact_limit;
    EXPECT_EQ(calculate_lzss_sparse_hash_tree_workspace(
                  5, {}, limits, 1).error,
              LzssSparseHashTreeError::none);

    --limits.max_internal_buffered_bytes;
    --limits.max_block_size;
    EXPECT_EQ(calculate_lzss_sparse_hash_tree_workspace(
                  5, {}, limits, 1).error,
              LzssSparseHashTreeError::workspace_limit_exceeded);
}

TEST(LzssSparseHashTreePool, AllocatesExhaustsReleasesAndReusesLifo) {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        8, {}, {}, 3);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeNodePool pool{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  8, {}, {}, 3,
                  storage.bytes.first(required.workspace_size), pool),
              LzssSparseHashTreeError::none);
    EXPECT_TRUE(pool.initialized());
    EXPECT_TRUE(pool.state_valid());
    EXPECT_EQ(pool.capacity(), 3U);
    EXPECT_EQ(pool.free_count(), 3U);
    EXPECT_EQ(pool.active_count(), 0U);

    const auto first = pool.allocate();
    const auto second = pool.allocate();
    const auto third = pool.allocate();
    ASSERT_TRUE(first.allocated);
    ASSERT_TRUE(second.allocated);
    ASSERT_TRUE(third.allocated);
    EXPECT_EQ(first.node, 0U);
    EXPECT_EQ(second.node, 1U);
    EXPECT_EQ(third.node, 2U);
    EXPECT_EQ(pool.free_count(), 0U);
    EXPECT_EQ(pool.active_count(), 3U);

    const auto exhausted = pool.allocate();
    EXPECT_FALSE(exhausted.allocated);
    EXPECT_EQ(exhausted.error, LzssSparseHashTreeError::none);
    EXPECT_TRUE(pool.state_valid());
    EXPECT_EQ(pool.free_count(), 0U);
    EXPECT_EQ(pool.active_count(), 3U);

    ASSERT_EQ(pool.release(second.node), LzssSparseHashTreeError::none);
    EXPECT_EQ(pool.free_count(), 1U);
    EXPECT_EQ(pool.active_count(), 2U);
    const auto reused = pool.allocate();
    EXPECT_TRUE(reused.allocated);
    EXPECT_EQ(reused.node, second.node);
    EXPECT_EQ(pool.free_count(), 0U);
    EXPECT_EQ(pool.active_count(), 3U);
}

TEST(LzssSparseHashTreePool, InitializesZeroCapacityAndExhaustsNormally) {
    LzssSparseHashTreeNodePool pool{};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  4, {}, {}, 0, {}, pool),
              LzssSparseHashTreeError::none);
    EXPECT_TRUE(pool.initialized());
    EXPECT_TRUE(pool.state_valid());
    EXPECT_EQ(pool.capacity(), 0U);
    EXPECT_EQ(pool.free_count(), 0U);
    EXPECT_EQ(pool.active_count(), 0U);
    const auto allocation = pool.allocate();
    EXPECT_FALSE(allocation.allocated);
    EXPECT_EQ(allocation.error, LzssSparseHashTreeError::none);
    EXPECT_TRUE(pool.state_valid());
}

TEST(LzssSparseHashTreePool, RejectsDoubleReleaseAndKeepsStickyError) {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, {}, 1);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeNodePool pool{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  5, {}, {}, 1,
                  storage.bytes.first(required.workspace_size), pool),
              LzssSparseHashTreeError::none);
    const auto allocated = pool.allocate();
    ASSERT_TRUE(allocated.allocated);
    ASSERT_EQ(pool.release(allocated.node), LzssSparseHashTreeError::none);
    EXPECT_EQ(pool.release(allocated.node),
              LzssSparseHashTreeError::double_release);
    EXPECT_FALSE(pool.state_valid());
    EXPECT_EQ(pool.last_error(), LzssSparseHashTreeError::double_release);
    EXPECT_EQ(pool.allocate().error,
              LzssSparseHashTreeError::double_release);
}

TEST(LzssSparseHashTreePool, RejectsOutOfRangeRelease) {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, {}, 1);
    ASSERT_EQ(required.error, LzssSparseHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssSparseHashTreeNodePool pool{};
    ASSERT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  5, {}, {}, 1,
                  storage.bytes.first(required.workspace_size), pool),
              LzssSparseHashTreeError::none);
    EXPECT_EQ(pool.release(1), LzssSparseHashTreeError::invalid_node);
    EXPECT_FALSE(pool.state_valid());
}

TEST(LzssSparseHashTreePool, RejectsShortAndMisalignedWorkspace) {
    const auto required = calculate_lzss_sparse_hash_tree_workspace(
        5, {}, {}, 1);
    ASSERT_GT(required.workspace_size, 1U);
    auto storage = make_storage(required.workspace_size + 1U);
    LzssSparseHashTreeNodePool pool{};
    EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  5, {}, {}, 1,
                  storage.bytes.first(required.workspace_size - 1U), pool),
              LzssSparseHashTreeError::workspace_too_small);
    EXPECT_FALSE(pool.initialized());

    EXPECT_EQ(initialize_lzss_sparse_hash_tree_node_pool(
                  5, {}, {}, 1,
                  storage.bytes.subspan(1, required.workspace_size), pool),
              LzssSparseHashTreeError::misaligned_workspace);
    EXPECT_FALSE(pool.initialized());
}

} // namespace

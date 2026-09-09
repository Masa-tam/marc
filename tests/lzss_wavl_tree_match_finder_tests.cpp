#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_wavl_tree_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

struct AlignedStorage {
    std::vector<std::max_align_t> words{};
    std::span<std::byte> bytes{};
};

[[nodiscard]] AlignedStorage make_storage(const std::size_t byte_count) {
    AlignedStorage result{};
    const auto word_count = byte_count == 0 ? 0
        : (byte_count + sizeof(std::max_align_t) - 1)
            / sizeof(std::max_align_t);
    result.words.resize(word_count);
    result.bytes = std::as_writable_bytes(std::span{result.words});
    return result;
}

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result{};
    result.reserve(text.size());
    for (const auto value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

template<typename T>
[[nodiscard]] std::span<const T> array_at(
    const std::span<const std::byte> workspace, const std::size_t offset,
    const std::size_t count) {
    return {reinterpret_cast<const T*>(workspace.data() + offset), count};
}

void expect_equal_extent(
    const std::size_t input_size, const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits) {
    const auto avl = calculate_lzss_binary_tree_workspace(
        input_size, parameters, limits);
    const auto wavl = calculate_lzss_wavl_tree_workspace(
        input_size, parameters, limits);
    ASSERT_EQ(avl.error, LzssBinaryTreeError::none);
    ASSERT_EQ(wavl.error, LzssWavlTreeError::none);
    EXPECT_EQ(wavl.workspace_size, avl.workspace_size);
    EXPECT_EQ(wavl.workspace_alignment, avl.workspace_alignment);
    EXPECT_EQ(wavl.node_count, avl.node_count);
    EXPECT_EQ(wavl.left_offset, avl.left_offset);
    EXPECT_EQ(wavl.right_offset, avl.right_offset);
    EXPECT_EQ(wavl.parent_offset, avl.parent_offset);
    EXPECT_EQ(wavl.rank_offset, avl.height_offset);
    EXPECT_EQ(wavl.position_offset, avl.position_offset);
    EXPECT_EQ(wavl.subtree_maximum_position_offset,
              avl.subtree_maximum_position_offset);
}

TEST(LzssWavlTreeMatchFinder, CalculatesExactlyAvlSizedWorkspace) {
    for (const auto input_size : std::array<std::size_t, 6>{
             0U, 4U, 5U, 17U, 65'536U, 1U << 20}) {
        LzssParameters parameters{};
        if (input_size > parameters.window_size) {
            parameters.window_size = static_cast<std::uint32_t>(input_size);
        }
        auto limits = marc::core::DecoderLimits{};
        limits.max_frame_size = std::max<std::uint64_t>(
            limits.max_frame_size, input_size);
        limits.max_lz_distance = std::max<std::uint64_t>(
            limits.max_lz_distance, parameters.window_size);
        limits.max_internal_buffered_bytes = UINT64_C(64) << 30;
        expect_equal_extent(input_size, parameters, limits);
    }
}

TEST(LzssWavlTreeMatchFinder, CalculatesSeparatedBoundedWorkspace) {
    const auto required = calculate_lzss_wavl_tree_workspace(5, {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    EXPECT_EQ(required.node_count, 5U);
    EXPECT_EQ(required.left_offset, 0U);
    EXPECT_EQ(required.right_offset, 20U);
    EXPECT_EQ(required.parent_offset, 40U);
    EXPECT_EQ(required.rank_offset, 60U);
    const auto position_offset = (65U + alignof(std::size_t) - 1U)
        / alignof(std::size_t) * alignof(std::size_t);
    EXPECT_EQ(required.position_offset, position_offset);
    EXPECT_EQ(required.subtree_maximum_position_offset,
              position_offset + 5U * sizeof(std::size_t));
    EXPECT_EQ(required.workspace_size,
              required.subtree_maximum_position_offset
                  + 5U * sizeof(std::size_t));
}

TEST(LzssWavlTreeMatchFinder, EnforcesAggregateLimitAtExactBoundary) {
    const auto baseline = calculate_lzss_wavl_tree_workspace(65'536, {}, {});
    ASSERT_EQ(baseline.error, LzssWavlTreeError::none);
    const auto aggregate = 65'536U + baseline.workspace_size;

    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = aggregate;
    EXPECT_EQ(calculate_lzss_wavl_tree_workspace(
                  65'536, {}, limits).error,
              LzssWavlTreeError::none);
    limits.max_internal_buffered_bytes = aggregate - 1U;
    EXPECT_EQ(calculate_lzss_wavl_tree_workspace(
                  65'536, {}, limits).error,
              LzssWavlTreeError::workspace_limit_exceeded);
}

TEST(LzssWavlTreeMatchFinder, InitializesEveryArrayAsCanonicalEmpty) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    auto storage = make_storage(required.workspace_size + 16U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});

    LzssWavlTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssWavlTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), input.size());
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_wavl_tree_null_node);

    const auto active = std::span<const std::byte>{storage.bytes}
        .first(required.workspace_size);
    for (const auto offset : {required.left_offset, required.right_offset,
                              required.parent_offset}) {
        const auto links = array_at<std::uint32_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(links, [](const auto value) {
            return value == lzss_wavl_tree_null_node;
        }));
    }
    const auto ranks = array_at<std::uint8_t>(
        active, required.rank_offset, required.node_count);
    EXPECT_TRUE(std::ranges::all_of(ranks, [](const auto value) {
        return value == lzss_wavl_tree_inactive_rank;
    }));
    for (const auto offset : {required.position_offset,
                              required.subtree_maximum_position_offset}) {
        const auto positions = array_at<std::size_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(positions, [](const auto value) {
            return value == lzss_wavl_tree_no_position;
        }));
    }
    for (std::uint32_t node = 0; node < required.node_count; ++node) {
        EXPECT_EQ(inspect_lzss_wavl_tree_node(finder, node),
                  LzssWavlTreeNodeSnapshot{});
    }
    EXPECT_TRUE(std::ranges::all_of(
        storage.bytes.subspan(required.workspace_size), [](const auto value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzssWavlTreeMatchFinder, InitializesShortInputWithoutWorkspace) {
    const auto input = bytes("ABCD");
    LzssWavlTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, {}, finder),
              LzssWavlTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_wavl_tree_null_node);
}

TEST(LzssWavlTreeMatchFinder, RejectsWorkspaceFailuresAtomically) {
    const auto seed_input = bytes("ABCD");
    LzssWavlTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  seed_input, {}, {}, {}, finder),
              LzssWavlTreeError::none);

    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    auto storage = make_storage(required.workspace_size + 1U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    EXPECT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssWavlTreeError::workspace_too_small);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1U, required.workspace_size), finder),
              LzssWavlTreeError::misaligned_workspace);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    auto arena = make_storage(required.workspace_size + input.size());
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    const auto original_arena = std::vector<std::byte>(
        arena.bytes.begin(), arena.bytes.end());
    EXPECT_EQ(initialize_lzss_wavl_tree_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssWavlTreeError::overlapping_buffers);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(arena.bytes, original_arena));
}

TEST(LzssWavlTreeMatchFinder, RejectsInvalidAndUnboundedRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_wavl_tree_workspace(16, {}, limits).error,
              LzssWavlTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_wavl_tree_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssWavlTreeError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_wavl_tree_workspace(65, {}, limits).error,
              LzssWavlTreeError::input_limit_exceeded);

    limits = {};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    EXPECT_EQ(calculate_lzss_wavl_tree_workspace(
                  std::numeric_limits<std::size_t>::max(), {}, limits).error,
              LzssWavlTreeError::arithmetic_overflow);
}

} // namespace

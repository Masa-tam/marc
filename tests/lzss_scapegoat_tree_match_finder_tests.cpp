#include "dictionary/lzss_scapegoat_tree_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

TEST(LzssScapegoatTreeMatchFinder, DefaultConstructedFinderIsInert) {
    const LzssScapegoatTreeMatchFinder finder{};
    EXPECT_FALSE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), 0U);
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_EQ(finder.maximum_active_node_count(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_scapegoat_tree_null_node);
    EXPECT_EQ(finder.next_position(), 0U);
}

TEST(LzssScapegoatTreeMatchFinder, CalculatesSeparatedBoundedWorkspace) {
    auto required = calculate_lzss_scapegoat_tree_workspace(4, {}, {});
    EXPECT_EQ(required.error, LzssScapegoatTreeError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.node_count, 0U);

    required = calculate_lzss_scapegoat_tree_workspace(5, {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    EXPECT_EQ(required.node_count, 5U);
    EXPECT_EQ(required.left_offset, 0U);
    EXPECT_EQ(required.right_offset, 20U);
    EXPECT_EQ(required.parent_offset, 40U);
    EXPECT_EQ(required.subtree_size_offset, 60U);
    EXPECT_EQ(required.position_offset, 80U);
    EXPECT_EQ(required.subtree_maximum_position_offset,
              80U + 5U * sizeof(std::size_t));
    EXPECT_EQ(required.rebuild_scratch_offset,
              80U + 10U * sizeof(std::size_t));
    EXPECT_EQ(required.workspace_size,
              100U + 10U * sizeof(std::size_t));

    LzssParameters parameters{};
    parameters.window_size = 32;
    required = calculate_lzss_scapegoat_tree_workspace(
        128, parameters, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    EXPECT_EQ(required.node_count, 32U);
}

TEST(LzssScapegoatTreeMatchFinder, InitializesEveryRegionAsEmpty) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size + 16U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);

    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), input.size());
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_EQ(finder.maximum_active_node_count(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_scapegoat_tree_null_node);
    EXPECT_EQ(finder.next_position(), 0U);

    const auto active = std::span<const std::byte>{storage.bytes}
        .first(required.workspace_size);
    for (const auto offset : {
             required.left_offset, required.right_offset,
             required.parent_offset, required.rebuild_scratch_offset}) {
        const auto values = array_at<std::uint32_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(values, [](const auto value) {
            return value == lzss_scapegoat_tree_null_node;
        }));
    }
    const auto sizes = array_at<std::uint32_t>(
        active, required.subtree_size_offset, required.node_count);
    EXPECT_TRUE(std::ranges::all_of(sizes, [](const auto value) {
        return value == 0U;
    }));
    for (const auto offset : {
             required.position_offset,
             required.subtree_maximum_position_offset}) {
        const auto positions = array_at<std::size_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(positions, [](const auto value) {
            return value == lzss_scapegoat_tree_no_position;
        }));
    }
    EXPECT_TRUE(std::ranges::all_of(
        storage.bytes.subspan(required.workspace_size), [](const auto value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzssScapegoatTreeMatchFinder, InitializesShortInputWithoutWorkspace) {
    const auto input = bytes("ABCD");
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, {}, finder),
              LzssScapegoatTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), 0U);
}

TEST(LzssScapegoatTreeMatchFinder, AcceptsExactWorkspace) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    EXPECT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size), finder),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.node_capacity(), required.node_count);
}

TEST(LzssScapegoatTreeMatchFinder, RejectsWorkspaceFailuresAtomically) {
    const auto seed_input = bytes("ABCD");
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  seed_input, {}, {}, {}, finder),
              LzssScapegoatTreeError::none);

    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size + 1U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    EXPECT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssScapegoatTreeError::workspace_too_small);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1, required.workspace_size), finder),
              LzssScapegoatTreeError::misaligned_workspace);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    auto arena = make_storage(required.workspace_size + input.size());
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    const auto original_arena = std::vector<std::byte>(
        arena.bytes.begin(), arena.bytes.end());
    EXPECT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssScapegoatTreeError::overlapping_buffers);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(arena.bytes, original_arena));
}

TEST(LzssScapegoatTreeMatchFinder, RejectsInvalidAndUnboundedRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_scapegoat_tree_workspace(16, {}, limits).error,
              LzssScapegoatTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_scapegoat_tree_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssScapegoatTreeError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_scapegoat_tree_workspace(65, {}, limits).error,
              LzssScapegoatTreeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 1U << 20;
    parameters = {};
    parameters.window_size = 65'536;
    EXPECT_EQ(calculate_lzss_scapegoat_tree_workspace(
                  65'536, parameters, limits).error,
              LzssScapegoatTreeError::workspace_limit_exceeded);

    limits = {};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    parameters = {};
    EXPECT_EQ(calculate_lzss_scapegoat_tree_workspace(
                  std::numeric_limits<std::size_t>::max(), parameters,
                  limits).error,
              LzssScapegoatTreeError::arithmetic_overflow);
}

} // namespace

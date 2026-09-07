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

template<typename T>
[[nodiscard]] std::span<T> mutable_array_at(
    const std::span<std::byte> workspace, const std::size_t offset,
    const std::size_t count) {
    return {reinterpret_cast<T*>(workspace.data() + offset), count};
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

TEST(LzssScapegoatTreeMatchFinder, InsertsFixedSlotsAndUpdatesMetadata) {
    const auto input = bytes("C0000000A0000000B0000000D0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);

    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(finder.active_node_count(), 4U);
    EXPECT_EQ(finder.maximum_active_node_count(), 4U);

    const auto root = inspect_lzss_scapegoat_tree_node(finder, 0);
    EXPECT_EQ(root.left, 8U);
    EXPECT_EQ(root.right, 24U);
    EXPECT_EQ(root.parent, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(root.subtree_size, 4U);
    EXPECT_EQ(root.position, 0U);
    EXPECT_EQ(root.subtree_maximum_position, 24U);

    const auto left = inspect_lzss_scapegoat_tree_node(finder, 8);
    EXPECT_EQ(left.left, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(left.right, 16U);
    EXPECT_EQ(left.parent, 0U);
    EXPECT_EQ(left.subtree_size, 2U);
    EXPECT_EQ(left.subtree_maximum_position, 16U);

    const auto middle = inspect_lzss_scapegoat_tree_node(finder, 16);
    EXPECT_EQ(middle.parent, 8U);
    EXPECT_EQ(middle.subtree_size, 1U);
    EXPECT_EQ(middle.subtree_maximum_position, 16U);

    const auto right = inspect_lzss_scapegoat_tree_node(finder, 24);
    EXPECT_EQ(right.parent, 0U);
    EXPECT_EQ(right.subtree_size, 1U);
    EXPECT_EQ(right.subtree_maximum_position, 24U);

    const auto scratch = array_at<std::uint32_t>(
        std::span<const std::byte>{storage.bytes},
        required.rebuild_scratch_offset, required.node_count);
    EXPECT_TRUE(std::ranges::all_of(scratch, [](const auto value) {
        return value == lzss_scapegoat_tree_null_node;
    }));
}

TEST(LzssScapegoatTreeMatchFinder, OrdersEqualCappedSuffixByPosition) {
    const auto input = bytes("ABCDE___ABCDE___ABCDE___");
    LzssParameters parameters{};
    parameters.max_match_length = 5;
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);

    for (const auto position : {16U, 0U, 8U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    EXPECT_EQ(finder.root_index(), 16U);
    const auto root = inspect_lzss_scapegoat_tree_node(finder, 16);
    EXPECT_EQ(root.left, 0U);
    EXPECT_EQ(root.right, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(root.subtree_size, 3U);
    EXPECT_EQ(root.subtree_maximum_position, 16U);
    const auto left = inspect_lzss_scapegoat_tree_node(finder, 0);
    EXPECT_EQ(left.right, 8U);
    EXPECT_EQ(left.subtree_size, 2U);
    EXPECT_EQ(left.subtree_maximum_position, 8U);
}

TEST(LzssScapegoatTreeMatchFinder, RejectsInvalidInsertionAtomically) {
    LzssScapegoatTreeMatchFinder uninitialized{};
    EXPECT_EQ(insert_lzss_scapegoat_tree_position(uninitialized, 0),
              LzssScapegoatTreeError::invalid_state);

    const auto input = bytes("ABCDE___FGHIJ___KLMNO___");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);

    std::vector<LzssScapegoatTreeNodeSnapshot> before{};
    for (std::uint32_t node = 0; node < required.node_count; ++node) {
        before.push_back(inspect_lzss_scapegoat_tree_node(finder, node));
    }
    const auto active_before = finder.active_node_count();
    const auto maximum_before = finder.maximum_active_node_count();
    const auto root_before = finder.root_index();

    EXPECT_EQ(insert_lzss_scapegoat_tree_position(
                  finder, input.size() - 4U),
              LzssScapegoatTreeError::invalid_position);
    EXPECT_EQ(insert_lzss_scapegoat_tree_position(finder, input.size()),
              LzssScapegoatTreeError::invalid_position);
    EXPECT_EQ(insert_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::invalid_state);
    EXPECT_EQ(insert_lzss_scapegoat_tree_position(finder, 8),
              LzssScapegoatTreeError::invalid_state);

    EXPECT_EQ(finder.active_node_count(), active_before);
    EXPECT_EQ(finder.maximum_active_node_count(), maximum_before);
    EXPECT_EQ(finder.root_index(), root_before);
    for (std::uint32_t node = 0; node < required.node_count; ++node) {
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, node),
                  before[node]) << node;
    }
}

TEST(LzssScapegoatTreeMatchFinder, RebuildsEvenTreeWithLowerMedian) {
    const auto input = bytes("A0000000B0000000C0000000D0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    ASSERT_EQ(rebuild_lzss_scapegoat_tree_subtree(
                  finder, finder.root_index()),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.root_index(), 8U);
    EXPECT_EQ(finder.active_node_count(), 4U);
    EXPECT_EQ(finder.maximum_active_node_count(), 4U);

    const auto root = inspect_lzss_scapegoat_tree_node(finder, 8);
    EXPECT_EQ(root.left, 0U);
    EXPECT_EQ(root.right, 16U);
    EXPECT_EQ(root.parent, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(root.subtree_size, 4U);
    EXPECT_EQ(root.subtree_maximum_position, 24U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).parent, 8U);
    const auto right = inspect_lzss_scapegoat_tree_node(finder, 16);
    EXPECT_EQ(right.parent, 8U);
    EXPECT_EQ(right.right, 24U);
    EXPECT_EQ(right.subtree_size, 2U);
    EXPECT_EQ(right.subtree_maximum_position, 24U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 24).parent, 16U);

    const auto scratch = array_at<std::uint32_t>(
        std::span<const std::byte>{storage.bytes},
        required.rebuild_scratch_offset, required.node_count);
    ASSERT_GE(scratch.size(), 4U);
    EXPECT_EQ(scratch[0], 0U);
    EXPECT_EQ(scratch[1], 8U);
    EXPECT_EQ(scratch[2], 16U);
    EXPECT_EQ(scratch[3], 24U);
}

TEST(LzssScapegoatTreeMatchFinder, RebuildsInteriorAndRepairsAncestors) {
    const auto input = bytes(
        "A0000000B0000000C0000000D0000000E0000000F0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U, 32U, 40U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    ASSERT_EQ(rebuild_lzss_scapegoat_tree_subtree(finder, 16),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 8).right, 24U);
    const auto rebuilt = inspect_lzss_scapegoat_tree_node(finder, 24);
    EXPECT_EQ(rebuilt.parent, 8U);
    EXPECT_EQ(rebuilt.left, 16U);
    EXPECT_EQ(rebuilt.right, 32U);
    EXPECT_EQ(rebuilt.subtree_size, 4U);
    EXPECT_EQ(rebuilt.subtree_maximum_position, 40U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 32).right, 40U);
    const auto root = inspect_lzss_scapegoat_tree_node(finder, 0);
    EXPECT_EQ(root.subtree_size, 6U);
    EXPECT_EQ(root.subtree_maximum_position, 40U);
}

TEST(LzssScapegoatTreeMatchFinder, InsertionTriggersFirstHeavyAncestor) {
    const auto input = bytes(
        "A0000000B0000000C0000000D0000000E0000000"
        "F0000000G0000000H0000000I0000000J0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (std::size_t position = 0; position < 80U; position += 8U) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none) << position;
    }

    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(finder.active_node_count(), 10U);
    EXPECT_EQ(finder.maximum_active_node_count(), 10U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 40).right, 56U);
    const auto rebuilt = inspect_lzss_scapegoat_tree_node(finder, 56);
    EXPECT_EQ(rebuilt.left, 48U);
    EXPECT_EQ(rebuilt.right, 64U);
    EXPECT_EQ(rebuilt.subtree_size, 4U);
    EXPECT_EQ(rebuilt.subtree_maximum_position, 72U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 64).right, 72U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).subtree_size, 10U);
}

TEST(LzssScapegoatTreeMatchFinder, ImpossibleRebuildIsSticky) {
    LzssScapegoatTreeMatchFinder uninitialized{};
    EXPECT_EQ(rebuild_lzss_scapegoat_tree_subtree(uninitialized, 0),
              LzssScapegoatTreeError::invalid_state);

    const auto input = bytes("A0000000B0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(rebuild_lzss_scapegoat_tree_subtree(finder, 8),
              LzssScapegoatTreeError::invalid_state);
    EXPECT_TRUE(finder.state_valid());

    auto sizes = mutable_array_at<std::uint32_t>(
        storage.bytes, required.subtree_size_offset, required.node_count);
    sizes[0] = static_cast<std::uint32_t>(required.node_count + 1U);
    EXPECT_EQ(rebuild_lzss_scapegoat_tree_subtree(finder, 0),
              LzssScapegoatTreeError::invalid_state);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(insert_lzss_scapegoat_tree_position(finder, 8),
              LzssScapegoatTreeError::invalid_state);
}

TEST(LzssScapegoatTreeMatchFinder, RemovesLeafAndOneChildPhysically) {
    const auto input = bytes("B0000000A0000000C0000000D0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);

    {
        auto storage = make_storage(required.workspace_size);
        LzssScapegoatTreeMatchFinder finder{};
        ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                      input, {}, {}, storage.bytes, finder),
                  LzssScapegoatTreeError::none);
        for (const auto position : {0U, 8U, 16U, 24U}) {
            ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                      LzssScapegoatTreeError::none);
        }
        ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 8),
                  LzssScapegoatTreeError::none);
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).left,
                  lzss_scapegoat_tree_null_node);
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 8),
                  LzssScapegoatTreeNodeSnapshot{});
        EXPECT_EQ(finder.active_node_count(), 3U);
        EXPECT_EQ(finder.maximum_active_node_count(), 4U);
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).subtree_size,
                  3U);
    }

    {
        auto storage = make_storage(required.workspace_size);
        LzssScapegoatTreeMatchFinder finder{};
        ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                      input, {}, {}, storage.bytes, finder),
                  LzssScapegoatTreeError::none);
        for (const auto position : {0U, 8U, 16U, 24U}) {
            ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                      LzssScapegoatTreeError::none);
        }
        ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 16),
                  LzssScapegoatTreeError::none);
        const auto root = inspect_lzss_scapegoat_tree_node(finder, 0);
        EXPECT_EQ(root.right, 24U);
        EXPECT_EQ(root.subtree_size, 3U);
        EXPECT_EQ(root.subtree_maximum_position, 24U);
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 24).parent, 0U);
        EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 16),
                  LzssScapegoatTreeNodeSnapshot{});
        EXPECT_EQ(finder.maximum_active_node_count(), 4U);
    }
}

TEST(LzssScapegoatTreeMatchFinder, TransplantsTwoChildSuccessorSlot) {
    const auto input = bytes(
        "C0000000A0000000E0000000D0000000F0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U, 32U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.root_index(), 24U);
    EXPECT_EQ(finder.active_node_count(), 4U);
    EXPECT_EQ(finder.maximum_active_node_count(), 5U);
    const auto root = inspect_lzss_scapegoat_tree_node(finder, 24);
    EXPECT_EQ(root.position, 24U);
    EXPECT_EQ(root.left, 8U);
    EXPECT_EQ(root.right, 16U);
    EXPECT_EQ(root.parent, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(root.subtree_size, 4U);
    EXPECT_EQ(root.subtree_maximum_position, 32U);
    const auto right = inspect_lzss_scapegoat_tree_node(finder, 16);
    EXPECT_EQ(right.left, lzss_scapegoat_tree_null_node);
    EXPECT_EQ(right.right, 32U);
    EXPECT_EQ(right.parent, 24U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0),
              LzssScapegoatTreeNodeSnapshot{});
}

TEST(LzssScapegoatTreeMatchFinder, WholeRebuildUsesStrictDeletionBoundary) {
    const auto input = bytes(
        "A0000000B0000000C0000000D0000000E0000000F0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U, 32U, 40U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 40),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 32),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.active_node_count(), 4U);
    EXPECT_EQ(finder.maximum_active_node_count(), 6U);
    EXPECT_EQ(finder.root_index(), 0U);

    ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 24),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.active_node_count(), 3U);
    EXPECT_EQ(finder.maximum_active_node_count(), 3U);
    EXPECT_EQ(finder.root_index(), 8U);
    const auto root = inspect_lzss_scapegoat_tree_node(finder, 8);
    EXPECT_EQ(root.left, 0U);
    EXPECT_EQ(root.right, 16U);
    EXPECT_EQ(root.subtree_size, 3U);
    EXPECT_EQ(root.subtree_maximum_position, 16U);
}

TEST(LzssScapegoatTreeMatchFinder, EmptyTreeResetsQAndReusesSlot) {
    const auto input = bytes("A0000000B0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.root_index(), lzss_scapegoat_tree_null_node);
    EXPECT_EQ(finder.maximum_active_node_count(), 0U);

    ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, 8),
              LzssScapegoatTreeError::none);
    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(finder.maximum_active_node_count(), 1U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).position, 8U);
}

TEST(LzssScapegoatTreeMatchFinder, RejectsInvalidRemovalAtomically) {
    LzssScapegoatTreeMatchFinder uninitialized{};
    EXPECT_EQ(remove_lzss_scapegoat_tree_position(uninitialized, 0),
              LzssScapegoatTreeError::invalid_state);

    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::none);
    const auto before = inspect_lzss_scapegoat_tree_node(finder, 0);

    EXPECT_EQ(remove_lzss_scapegoat_tree_position(finder, 8),
              LzssScapegoatTreeError::invalid_state);
    EXPECT_EQ(remove_lzss_scapegoat_tree_position(
                  finder, input.size() - 4U),
              LzssScapegoatTreeError::invalid_position);
    EXPECT_EQ(remove_lzss_scapegoat_tree_position(finder, input.size()),
              LzssScapegoatTreeError::invalid_position);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0), before);
    EXPECT_EQ(finder.active_node_count(), 1U);
    EXPECT_EQ(finder.maximum_active_node_count(), 1U);
    EXPECT_TRUE(finder.state_valid());
}

TEST(LzssScapegoatTreeMatchFinder, ImpossibleRemovalLinksAreSticky) {
    const auto input = bytes("B0000000A0000000C0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }

    auto right = mutable_array_at<std::uint32_t>(
        storage.bytes, required.right_offset, required.node_count);
    right[0] = 8;
    EXPECT_EQ(remove_lzss_scapegoat_tree_position(finder, 0),
              LzssScapegoatTreeError::invalid_state);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.active_node_count(), 3U);
    EXPECT_EQ(inspect_lzss_scapegoat_tree_node(finder, 0).position, 0U);
    EXPECT_EQ(remove_lzss_scapegoat_tree_position(finder, 8),
              LzssScapegoatTreeError::invalid_state);
}

TEST(LzssScapegoatTreeMatchFinder, ValidatorAcceptsEverySmallMutation) {
    EXPECT_EQ(validate_lzss_scapegoat_tree(
                  LzssScapegoatTreeMatchFinder{}),
              LzssScapegoatTreeValidationError::uninitialized);

    const auto input = bytes(
        "A0000000B0000000C0000000D0000000E0000000"
        "F0000000G0000000H0000000I0000000J0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    ASSERT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::none);

    for (std::size_t position = 0; position < 80U; position += 8U) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none) << position;
        ASSERT_EQ(validate_lzss_scapegoat_tree(finder),
                  LzssScapegoatTreeValidationError::none) << position;
    }
    for (std::size_t position = 80U; position != 0;) {
        position -= 8U;
        ASSERT_EQ(remove_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none) << position;
        ASSERT_EQ(validate_lzss_scapegoat_tree(finder),
                  LzssScapegoatTreeValidationError::none) << position;
    }
    EXPECT_TRUE(finder.empty());
}

TEST(LzssScapegoatTreeMatchFinder,
     ValidatorDistinguishesIndependentWorkspaceCorruption) {
    const auto input = bytes("B0000000A0000000C0000000D0000000");
    const auto required = calculate_lzss_scapegoat_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssScapegoatTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssScapegoatTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_scapegoat_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssScapegoatTreeError::none);
    for (const auto position : {0U, 8U, 16U}) {
        ASSERT_EQ(insert_lzss_scapegoat_tree_position(finder, position),
                  LzssScapegoatTreeError::none);
    }
    ASSERT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::none);

    auto left = mutable_array_at<std::uint32_t>(
        storage.bytes, required.left_offset, required.node_count);
    auto right = mutable_array_at<std::uint32_t>(
        storage.bytes, required.right_offset, required.node_count);
    auto parent = mutable_array_at<std::uint32_t>(
        storage.bytes, required.parent_offset, required.node_count);
    auto sizes = mutable_array_at<std::uint32_t>(
        storage.bytes, required.subtree_size_offset, required.node_count);
    auto positions = mutable_array_at<std::size_t>(
        storage.bytes, required.position_offset, required.node_count);
    auto maxima = mutable_array_at<std::size_t>(
        storage.bytes, required.subtree_maximum_position_offset,
        required.node_count);

    left[1] = 0;
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_inactive_node);
    left[1] = lzss_scapegoat_tree_null_node;

    left[0] = static_cast<std::uint32_t>(required.node_count);
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_index);
    left[0] = 8;

    parent[8] = lzss_scapegoat_tree_null_node;
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_parent);
    parent[8] = 0;

    std::swap(left[0], right[0]);
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_order);
    std::swap(left[0], right[0]);

    ++sizes[0];
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_subtree_size);
    --sizes[0];

    const auto saved_maximum = maxima[0];
    maxima[0] = 8;
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_subtree_maximum);
    maxima[0] = saved_maximum;

    const auto saved_position = positions[8];
    positions[8] = 9;
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::invalid_slot_position);
    positions[8] = saved_position;

    left[0] = lzss_scapegoat_tree_null_node;
    sizes[0] = 2;
    parent[8] = 8;
    left[8] = 8;
    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::cycle_or_disconnected);
    left[8] = lzss_scapegoat_tree_null_node;
    parent[8] = 0;
    left[0] = 8;
    sizes[0] = 3;

    EXPECT_EQ(validate_lzss_scapegoat_tree(finder),
              LzssScapegoatTreeValidationError::none);
}

} // namespace

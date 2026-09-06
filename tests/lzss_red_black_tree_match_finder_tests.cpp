#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_red_black_tree_match_finder.hpp"

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

[[nodiscard]] constexpr std::size_t align_size(
    const std::size_t value, const std::size_t alignment) noexcept {
    const auto remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
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

void expect_three_node_tree(
    const std::string_view text, const std::array<std::size_t, 3>& order,
    const std::uint32_t expected_root, const std::uint32_t expected_left,
    const std::uint32_t expected_right,
    const LzssParameters& parameters = {}) {
    const auto input = bytes(text);
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : order) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
        ASSERT_EQ(validate_lzss_red_black_tree(finder),
                  LzssRedBlackTreeValidationError::none);
    }

    ASSERT_EQ(finder.root_index(), expected_root);
    const auto root = inspect_lzss_red_black_tree_node(finder, expected_root);
    EXPECT_EQ(root.left, expected_left);
    EXPECT_EQ(root.right, expected_right);
    EXPECT_EQ(root.parent, lzss_red_black_tree_null_node);
    EXPECT_EQ(root.color, LzssRedBlackTreeNodeColor::black);
    EXPECT_EQ(root.subtree_maximum_position, 16U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, expected_left).color,
              LzssRedBlackTreeNodeColor::red);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, expected_right).color,
              LzssRedBlackTreeNodeColor::red);
}

[[nodiscard]] LzssMatch exhaustive_match(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::size_t position) {
    if (position >= input.size()) return {};
    const auto first = position > parameters.window_size
        ? position - parameters.window_size : 0U;
    std::size_t best_position{};
    std::uint32_t best_length{};
    bool found{};
    for (auto candidate = first; candidate < position; ++candidate) {
        const auto maximum = std::min({
            input.size() - position,
            input.size() - candidate,
            static_cast<std::size_t>(parameters.max_match_length)});
        std::size_t length{};
        while (length < maximum
               && input[candidate + length] == input[position + length]) {
            ++length;
        }
        if (length > best_length
            || (length == best_length && found && candidate > best_position)) {
            best_position = candidate;
            best_length = static_cast<std::uint32_t>(length);
            found = true;
        }
    }
    if (!found || best_length < parameters.min_match_length) return {};
    return {static_cast<std::uint32_t>(position - best_position), best_length};
}

TEST(LzssRedBlackTreeMatchFinder, CalculatesSeparatedBoundedWorkspace) {
    auto required = calculate_lzss_red_black_tree_workspace(4, {}, {});
    EXPECT_EQ(required.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.node_count, 0U);

    required = calculate_lzss_red_black_tree_workspace(5, {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(required.node_count, 5U);
    EXPECT_EQ(required.left_offset, 0U);
    EXPECT_EQ(required.right_offset, 20U);
    EXPECT_EQ(required.parent_offset, 40U);
    EXPECT_EQ(required.color_offset, 60U);
    const auto expected_position_offset = align_size(65U, alignof(std::size_t));
    const auto expected_subtree_offset =
        expected_position_offset + 5U * sizeof(std::size_t);
    EXPECT_EQ(required.position_offset, expected_position_offset);
    EXPECT_EQ(required.subtree_maximum_position_offset,
              expected_subtree_offset);
    EXPECT_EQ(required.workspace_size,
              expected_subtree_offset + 5U * sizeof(std::size_t));

    LzssParameters parameters{};
    parameters.window_size = 32;
    required = calculate_lzss_red_black_tree_workspace(128, parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(required.node_count, 32U);

    const auto avl = calculate_lzss_binary_tree_workspace(
        128, parameters, {});
    ASSERT_EQ(avl.error, LzssBinaryTreeError::none);
    EXPECT_EQ(required.workspace_size, avl.workspace_size);
    EXPECT_EQ(required.workspace_alignment, avl.workspace_alignment);
}

TEST(LzssRedBlackTreeMatchFinder, InitializesEveryArrayAsEmpty) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size + 16U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);

    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), input.size());
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_red_black_tree_null_node);
    EXPECT_EQ(finder.next_position(), 0U);

    const auto active = std::span<const std::byte>{storage.bytes}
        .first(required.workspace_size);
    for (const auto offset : {required.left_offset, required.right_offset,
                              required.parent_offset}) {
        const auto links = array_at<std::uint32_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(links, [](const auto value) {
            return value == lzss_red_black_tree_null_node;
        }));
    }
    const auto colors = array_at<LzssRedBlackTreeNodeColor>(
        active, required.color_offset, required.node_count);
    EXPECT_TRUE(std::ranges::all_of(colors, [](const auto value) {
        return value == LzssRedBlackTreeNodeColor::inactive;
    }));
    for (const auto offset : {required.position_offset,
                              required.subtree_maximum_position_offset}) {
        const auto positions = array_at<std::size_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(positions, [](const auto value) {
            return value == std::numeric_limits<std::size_t>::max();
        }));
    }
    EXPECT_TRUE(std::ranges::all_of(
        storage.bytes.subspan(required.workspace_size), [](const auto value) {
            return value == std::byte{0xa5};
        }));
}

TEST(LzssRedBlackTreeMatchFinder, InitializesShortInputWithoutWorkspace) {
    const auto input = bytes("ABCD");
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, {}, finder),
              LzssRedBlackTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_red_black_tree_null_node);
}

TEST(LzssRedBlackTreeMatchFinder, RejectsWorkspaceFailuresAtomically) {
    const auto seed_input = bytes("ABCD");
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  seed_input, {}, {}, {}, finder),
              LzssRedBlackTreeError::none);

    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size + 1U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    EXPECT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssRedBlackTreeError::workspace_too_small);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1, required.workspace_size), finder),
              LzssRedBlackTreeError::misaligned_workspace);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    auto arena = make_storage(required.workspace_size + input.size());
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    const auto original_arena = std::vector<std::byte>(
        arena.bytes.begin(), arena.bytes.end());
    EXPECT_EQ(initialize_lzss_red_black_tree_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssRedBlackTreeError::overlapping_buffers);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(arena.bytes, original_arena));
}

TEST(LzssRedBlackTreeMatchFinder, RejectsInvalidAndUnboundedRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_red_black_tree_workspace(16, {}, limits).error,
              LzssRedBlackTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_red_black_tree_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssRedBlackTreeError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_red_black_tree_workspace(65, {}, limits).error,
              LzssRedBlackTreeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 1U << 20;
    parameters = {};
    parameters.window_size = 65'536;
    EXPECT_EQ(calculate_lzss_red_black_tree_workspace(
                  65'536, parameters, limits).error,
              LzssRedBlackTreeError::workspace_limit_exceeded);

    limits = {};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    parameters = {};
    EXPECT_EQ(calculate_lzss_red_black_tree_workspace(
                  std::numeric_limits<std::size_t>::max(), parameters,
                  limits).error,
              LzssRedBlackTreeError::arithmetic_overflow);
}

TEST(LzssRedBlackTreeMatchFinder, InsertsWithDeterministicSingleRotations) {
    expect_three_node_tree(
        "C0000000B0000000A0000000", {0, 8, 16}, 8, 16, 0);
    expect_three_node_tree(
        "A0000000B0000000C0000000", {0, 8, 16}, 8, 0, 16);
}

TEST(LzssRedBlackTreeMatchFinder, InsertsWithDeterministicDoubleRotations) {
    expect_three_node_tree(
        "C0000000A0000000B0000000", {0, 8, 16}, 16, 8, 0);
    expect_three_node_tree(
        "A0000000C0000000B0000000", {0, 8, 16}, 16, 0, 8);
}

TEST(LzssRedBlackTreeMatchFinder, RecolorsWithoutChangingSlotIdentity) {
    const auto input = bytes("A0000000B0000000C0000000D0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
        ASSERT_EQ(validate_lzss_red_black_tree(finder),
                  LzssRedBlackTreeValidationError::none);
    }

    EXPECT_EQ(finder.root_index(), 8U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).color,
              LzssRedBlackTreeNodeColor::black);
    const auto right = inspect_lzss_red_black_tree_node(finder, 16);
    EXPECT_EQ(right.color, LzssRedBlackTreeNodeColor::black);
    EXPECT_EQ(right.right, 24U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 24).color,
              LzssRedBlackTreeNodeColor::red);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 24).position, 24U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 8)
                  .subtree_maximum_position,
              24U);
}

TEST(LzssRedBlackTreeMatchFinder, OrdersEqualCappedSuffixByPosition) {
    LzssParameters parameters{};
    parameters.max_match_length = 5;
    expect_three_node_tree(
        "ABCDE___ABCDE___ABCDE___", {0, 8, 16}, 8, 0, 16,
        parameters);
}

TEST(LzssRedBlackTreeMatchFinder, RejectsInvalidInsertionAtomically) {
    LzssRedBlackTreeMatchFinder uninitialized{};
    EXPECT_EQ(insert_lzss_red_black_tree_position(uninitialized, 0),
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(validate_lzss_red_black_tree(uninitialized),
              LzssRedBlackTreeValidationError::uninitialized);

    const auto input = bytes("ABCDE___FGHIJ___KLMNO___");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(insert_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::none);
    const auto root_before = inspect_lzss_red_black_tree_node(
        finder, finder.root_index());

    EXPECT_EQ(insert_lzss_red_black_tree_position(
                  finder, input.size() - 4U),
              LzssRedBlackTreeError::invalid_position);
    EXPECT_EQ(insert_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(insert_lzss_red_black_tree_position(finder, 8),
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, finder.root_index()),
              root_before);
    EXPECT_EQ(finder.active_node_count(), 1U);
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
}

TEST(LzssRedBlackTreeMatchFinder, ValidatorDetectsIndependentCorruption) {
    const auto input = bytes("A0000000B0000000C0000000D0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
    }
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);

    auto left = mutable_array_at<std::uint32_t>(
        storage.bytes, required.left_offset, required.node_count);
    auto parent = mutable_array_at<std::uint32_t>(
        storage.bytes, required.parent_offset, required.node_count);
    auto color = mutable_array_at<LzssRedBlackTreeNodeColor>(
        storage.bytes, required.color_offset, required.node_count);
    auto position = mutable_array_at<std::size_t>(
        storage.bytes, required.position_offset, required.node_count);
    auto maximum = mutable_array_at<std::size_t>(
        storage.bytes, required.subtree_maximum_position_offset,
        required.node_count);
    const auto root = finder.root_index();
    const auto root_left = left[root];

    color[root] = LzssRedBlackTreeNodeColor::red;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_root);
    color[root] = LzssRedBlackTreeNodeColor::black;

    color[0] = LzssRedBlackTreeNodeColor::red;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_black_height);
    color[0] = LzssRedBlackTreeNodeColor::black;

    color[24] = LzssRedBlackTreeNodeColor::black;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_black_height);
    color[24] = LzssRedBlackTreeNodeColor::red;

    color[16] = LzssRedBlackTreeNodeColor::red;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::red_parent_violation);
    color[16] = LzssRedBlackTreeNodeColor::black;

    maximum[root] = 16;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_subtree_maximum);
    maximum[root] = 24;

    const auto saved_position = position[root_left];
    position[root_left] = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_slot_position);
    position[root_left] = saved_position;

    parent[root_left] = 16;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_parent);
    parent[root_left] = root;

    left[root] = static_cast<std::uint32_t>(required.node_count);
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_index);
    left[root] = root_left;

    left[root] = 16;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_order);
    left[root] = root_left;

    left[7] = 0;
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_inactive_node);
    left[7] = lzss_red_black_tree_null_node;

    color[7] = static_cast<LzssRedBlackTreeNodeColor>(17);
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::invalid_color);
    color[7] = LzssRedBlackTreeNodeColor::inactive;
}

TEST(LzssRedBlackTreeMatchFinder, DeterministicallyInsertsFixedSeedInput) {
    std::vector<std::byte> input(512);
    std::uint32_t state = UINT32_C(0x13579bdf);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto first_storage = make_storage(required.workspace_size);
    auto second_storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder first{};
    LzssRedBlackTreeMatchFinder second{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, first_storage.bytes, first),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssRedBlackTreeError::none);

    const auto count = input.size() - lzss_red_black_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(first, position),
                  LzssRedBlackTreeError::none) << position;
        ASSERT_EQ(insert_lzss_red_black_tree_position(second, position),
                  LzssRedBlackTreeError::none) << position;
        ASSERT_EQ(validate_lzss_red_black_tree(first),
                  LzssRedBlackTreeValidationError::none) << position;
        ASSERT_EQ(validate_lzss_red_black_tree(second),
                  LzssRedBlackTreeValidationError::none) << position;
    }
    EXPECT_EQ(first.root_index(), second.root_index());
    EXPECT_EQ(first.active_node_count(), count);
    for (std::uint32_t node = 0; node < required.node_count; ++node) {
        EXPECT_EQ(inspect_lzss_red_black_tree_node(first, node),
                  inspect_lzss_red_black_tree_node(second, node)) << node;
    }
}

TEST(LzssRedBlackTreeMatchFinder, RemovesBlackLeafWithDeterministicRepair) {
    const auto input = bytes("A0000000B0000000C0000000D0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
    }
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);

    ASSERT_EQ(remove_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 16U);
    const auto root = inspect_lzss_red_black_tree_node(finder, 16);
    EXPECT_EQ(root.left, 8U);
    EXPECT_EQ(root.right, 24U);
    EXPECT_EQ(root.subtree_maximum_position, 24U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).color,
              LzssRedBlackTreeNodeColor::inactive);
}

TEST(LzssRedBlackTreeMatchFinder, TransplantsDirectRootSuccessor) {
    const auto input = bytes("A0000000B0000000C0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : {0U, 8U, 16U}) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
    }
    ASSERT_EQ(finder.root_index(), 8U);

    ASSERT_EQ(remove_lzss_red_black_tree_position(finder, 8),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 16U);
    const auto root = inspect_lzss_red_black_tree_node(finder, 16);
    EXPECT_EQ(root.position, 16U);
    EXPECT_EQ(root.left, 0U);
    EXPECT_EQ(root.right, lzss_red_black_tree_null_node);
    EXPECT_EQ(root.color, LzssRedBlackTreeNodeColor::black);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 8).color,
              LzssRedBlackTreeNodeColor::inactive);
}

TEST(LzssRedBlackTreeMatchFinder,
     TransplantsNonDirectSuccessorWithoutPayloadSwap) {
    const auto input = bytes(
        "D0000000B0000000F0000000E0000000G0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U, 32U}) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                  LzssRedBlackTreeError::none);
    }
    ASSERT_EQ(finder.root_index(), 0U);

    ASSERT_EQ(remove_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 24U);
    const auto root = inspect_lzss_red_black_tree_node(finder, 24);
    EXPECT_EQ(root.position, 24U);
    EXPECT_EQ(root.left, 8U);
    EXPECT_EQ(root.right, 16U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 16).right, 32U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).color,
              LzssRedBlackTreeNodeColor::inactive);
}

TEST(LzssRedBlackTreeMatchFinder, RemovedSlotCanBeReused) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(insert_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(remove_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(insert_lzss_red_black_tree_position(finder, 8),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).position, 8U);
}

TEST(LzssRedBlackTreeMatchFinder, RejectsInvalidRemovalAtomically) {
    LzssRedBlackTreeMatchFinder uninitialized{};
    EXPECT_EQ(remove_lzss_red_black_tree_position(uninitialized, 0),
              LzssRedBlackTreeError::invalid_state);

    const auto input = bytes("A0000000B0000000C0000000");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(insert_lzss_red_black_tree_position(finder, 8),
              LzssRedBlackTreeError::none);
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());
    const auto original_root = finder.root_index();

    EXPECT_EQ(remove_lzss_red_black_tree_position(finder, 0),
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(remove_lzss_red_black_tree_position(
                  finder, input.size() - 4U),
              LzssRedBlackTreeError::invalid_position);
    EXPECT_EQ(finder.root_index(), original_root);
    EXPECT_EQ(finder.active_node_count(), 1U);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
}

TEST(LzssRedBlackTreeMatchFinder,
     BulkDeletionRemainsBalancedAndDeterministic) {
    std::vector<std::byte> input(256);
    std::uint32_t state = UINT32_C(0x2468ace1);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto first_storage = make_storage(required.workspace_size);
    auto second_storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder first{};
    LzssRedBlackTreeMatchFinder second{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, first_storage.bytes, first),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssRedBlackTreeError::none);
    const auto count = input.size() - lzss_red_black_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_red_black_tree_position(first, position),
                  LzssRedBlackTreeError::none);
        ASSERT_EQ(insert_lzss_red_black_tree_position(second, position),
                  LzssRedBlackTreeError::none);
    }

    for (std::size_t parity = 0; parity < 2; ++parity) {
        for (std::size_t position = parity; position < count; position += 2) {
            ASSERT_EQ(remove_lzss_red_black_tree_position(first, position),
                      LzssRedBlackTreeError::none) << position;
            ASSERT_EQ(remove_lzss_red_black_tree_position(second, position),
                      LzssRedBlackTreeError::none) << position;
            ASSERT_EQ(validate_lzss_red_black_tree(first),
                      LzssRedBlackTreeValidationError::none) << position;
            ASSERT_EQ(validate_lzss_red_black_tree(second),
                      LzssRedBlackTreeValidationError::none) << position;
            EXPECT_EQ(first.root_index(), second.root_index()) << position;
        }
        for (std::uint32_t node = 0; node < required.node_count; ++node) {
            EXPECT_EQ(inspect_lzss_red_black_tree_node(first, node),
                      inspect_lzss_red_black_tree_node(second, node)) << node;
        }
    }
    EXPECT_TRUE(first.empty());
    EXPECT_TRUE(second.empty());
    EXPECT_EQ(first.root_index(), lzss_red_black_tree_null_node);
}

TEST(LzssRedBlackTreeMatchFinder,
     FixedSeedRemovalOrdersPreserveEveryInvariant) {
    for (std::uint32_t trial = 0; trial < 8; ++trial) {
        std::vector<std::byte> input(96);
        auto state = UINT32_C(0x9e3779b9) ^ trial;
        for (auto& value : input) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            value = static_cast<std::byte>(state >> 24U);
        }
        const auto required = calculate_lzss_red_black_tree_workspace(
            input.size(), {}, {});
        ASSERT_EQ(required.error, LzssRedBlackTreeError::none) << trial;
        auto storage = make_storage(required.workspace_size);
        LzssRedBlackTreeMatchFinder finder{};
        ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                      input, {}, {}, storage.bytes, finder),
                  LzssRedBlackTreeError::none) << trial;
        const auto count =
            input.size() - lzss_red_black_tree_prefix_size + 1U;
        std::vector<std::size_t> removal_order(count);
        for (std::size_t position = 0; position < count; ++position) {
            removal_order[position] = position;
            ASSERT_EQ(insert_lzss_red_black_tree_position(finder, position),
                      LzssRedBlackTreeError::none) << trial << ':' << position;
        }
        for (std::size_t remaining = count; remaining > 1; --remaining) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            const auto selected = static_cast<std::size_t>(state) % remaining;
            std::swap(removal_order[selected], removal_order[remaining - 1]);
        }
        for (const auto position : removal_order) {
            ASSERT_EQ(remove_lzss_red_black_tree_position(finder, position),
                      LzssRedBlackTreeError::none) << trial << ':' << position;
            ASSERT_EQ(validate_lzss_red_black_tree(finder),
                      LzssRedBlackTreeValidationError::none)
                << trial << ':' << position;
        }
        EXPECT_TRUE(finder.empty()) << trial;
    }
}

TEST(LzssRedBlackTreeMatchFinder, FindsBothNeighborsAndTheirLcp) {
    const auto input = bytes("ABCDE___ABCDG___ABCDF___");
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    finder.advance(0, 16);
    ASSERT_TRUE(finder.state_valid());

    const auto result = finder.find_neighbors(16);
    EXPECT_EQ(result.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(result.predecessor_position, 0U);
    EXPECT_EQ(result.successor_position, 8U);
    EXPECT_EQ(result.predecessor_lcp, 4U);
    EXPECT_EQ(result.successor_lcp, 4U);
    EXPECT_EQ(result.maximum_lcp, 4U);
    EXPECT_EQ(finder.find_candidate(16),
              LzssRedBlackTreeCandidateQueryResult{});
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
}

TEST(LzssRedBlackTreeMatchFinder,
     PrefixIntervalSelectsNonadjacentNewestCandidate) {
    const auto input = bytes(
        "ABCDEA__ABCDEL__ABCDEN__ABCDEB__ABCDEM__");
    LzssParameters parameters{};
    parameters.window_size = 32;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    finder.advance(0, 32);
    ASSERT_TRUE(finder.state_valid());

    const auto neighbors = finder.find_neighbors(32);
    ASSERT_EQ(neighbors.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(neighbors.predecessor_position, 8U);
    EXPECT_EQ(neighbors.successor_position, 16U);
    EXPECT_EQ(neighbors.maximum_lcp, 5U);
    const auto candidate = finder.find_candidate(32);
    EXPECT_EQ(candidate.error, LzssRedBlackTreeError::none);
    EXPECT_EQ(candidate.candidate_position, 24U);
    EXPECT_EQ(candidate.length, 5U);
    EXPECT_EQ(finder.find_match(32), (LzssMatch{8, 5}));
}

TEST(LzssRedBlackTreeMatchFinder, QueryRejectsInvalidPreparedState) {
    LzssRedBlackTreeMatchFinder uninitialized{};
    EXPECT_EQ(uninitialized.find_neighbors(0).error,
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(uninitialized.find_candidate(0).error,
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(uninitialized.find_match(0), LzssMatch{});

    const auto input = bytes("ABCDEFGHIJKL");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    EXPECT_EQ(finder.find_neighbors(0),
              LzssRedBlackTreeNeighborQueryResult{});
    EXPECT_EQ(finder.find_neighbors(1).error,
              LzssRedBlackTreeError::invalid_state);
    EXPECT_EQ(finder.find_neighbors(input.size() + 1U).error,
              LzssRedBlackTreeError::invalid_position);
    EXPECT_EQ(finder.find_neighbors(input.size() - 4U).error,
              LzssRedBlackTreeError::invalid_state);
    finder.advance(0, input.size() - 4U);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.find_neighbors(input.size() - 4U),
              LzssRedBlackTreeNeighborQueryResult{});
}

TEST(LzssRedBlackTreeMatchFinder,
     ExactQueryMatchesEnumerationAcrossSlidingWindow) {
    std::vector<std::byte> input(512);
    std::uint32_t state = UINT32_C(0xc31d7a59);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 30U);
    }
    LzssParameters parameters{};
    parameters.window_size = 64;
    parameters.max_match_length = 64;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);

    std::size_t matching_queries{};
    const auto last_query = input.size() - lzss_red_black_tree_prefix_size;
    for (std::size_t position = 0; position <= last_query; ++position) {
        const auto expected = exhaustive_match(input, parameters, position);
        const auto actual = finder.find_match(position);
        EXPECT_EQ(actual, expected) << position;
        if (actual.length != 0) ++matching_queries;
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
        ASSERT_EQ(validate_lzss_red_black_tree(finder),
                  LzssRedBlackTreeValidationError::none) << position;
    }
    EXPECT_GT(matching_queries, 0U);
}

TEST(LzssRedBlackTreeMatchFinder,
     AdvancesSequentiallyAndRetainsExactWindow) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);

    finder.advance(0, 8);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), 8U);
    EXPECT_EQ(finder.active_node_count(), 8U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).position, 0U);
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);

    finder.advance(8, 9);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), 9U);
    EXPECT_EQ(finder.active_node_count(), 8U);
    EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, 0).position, 8U);
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
}

TEST(LzssRedBlackTreeMatchFinder, AdvancesThroughNonindexableTail) {
    const auto input = bytes("ABCDEFGHIJKL");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);

    finder.advance(0, input.size());
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), input.size());
    EXPECT_EQ(finder.active_node_count(), 4U);
    for (std::uint32_t slot = 0; slot < 4; ++slot) {
        EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, slot).color,
                  LzssRedBlackTreeNodeColor::inactive);
    }
    for (std::uint32_t slot = 4; slot < 8; ++slot) {
        EXPECT_EQ(inspect_lzss_red_black_tree_node(finder, slot).position,
                  slot);
    }
    EXPECT_EQ(validate_lzss_red_black_tree(finder),
              LzssRedBlackTreeValidationError::none);
}

TEST(LzssRedBlackTreeMatchFinder, AdvanceIsIndependentOfCallerChunking) {
    std::vector<std::byte> input(128);
    std::uint32_t state = UINT32_C(0x5a17c3e9);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto bulk_storage = make_storage(required.workspace_size);
    auto byte_storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder bulk{};
    LzssRedBlackTreeMatchFinder bytewise{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, bulk_storage.bytes, bulk),
              LzssRedBlackTreeError::none);
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, byte_storage.bytes, bytewise),
              LzssRedBlackTreeError::none);

    bulk.advance(0, input.size());
    for (std::size_t position = 0; position < input.size(); ++position) {
        bytewise.advance(position, position + 1U);
    }

    ASSERT_TRUE(bulk.state_valid());
    ASSERT_TRUE(bytewise.state_valid());
    EXPECT_EQ(validate_lzss_red_black_tree(bulk),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(validate_lzss_red_black_tree(bytewise),
              LzssRedBlackTreeValidationError::none);
    EXPECT_EQ(bulk.next_position(), bytewise.next_position());
    EXPECT_EQ(bulk.active_node_count(), bytewise.active_node_count());
    EXPECT_EQ(bulk.root_index(), bytewise.root_index());
    for (std::uint32_t node = 0; node < required.node_count; ++node) {
        EXPECT_EQ(inspect_lzss_red_black_tree_node(bulk, node),
                  inspect_lzss_red_black_tree_node(bytewise, node)) << node;
    }
}

TEST(LzssRedBlackTreeMatchFinder, InvalidAdvanceOrderIsSticky) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);

    auto wrong_storage = make_storage(required.workspace_size);
    std::ranges::fill(wrong_storage.bytes, std::byte{0xa5});
    LzssRedBlackTreeMatchFinder wrong{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, wrong_storage.bytes, wrong),
              LzssRedBlackTreeError::none);
    const auto original_storage = std::vector<std::byte>(
        wrong_storage.bytes.begin(), wrong_storage.bytes.end());
    wrong.advance(1, 2);
    EXPECT_FALSE(wrong.state_valid());
    EXPECT_EQ(wrong.next_position(), input.size());
    EXPECT_TRUE(std::ranges::equal(wrong_storage.bytes, original_storage));
    EXPECT_EQ(validate_lzss_red_black_tree(wrong),
              LzssRedBlackTreeValidationError::invalid_protocol_state);
    wrong.advance(input.size(), input.size());
    EXPECT_FALSE(wrong.state_valid());

    auto backward_storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder backward{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, backward_storage.bytes, backward),
              LzssRedBlackTreeError::none);
    backward.advance(0, 4);
    ASSERT_TRUE(backward.state_valid());
    backward.advance(4, 3);
    EXPECT_FALSE(backward.state_valid());
    EXPECT_EQ(validate_lzss_red_black_tree(backward),
              LzssRedBlackTreeValidationError::invalid_protocol_state);

    auto oversized_storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder oversized{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, parameters, {}, oversized_storage.bytes, oversized),
              LzssRedBlackTreeError::none);
    oversized.advance(0, input.size() + 1U);
    EXPECT_FALSE(oversized.state_valid());
    EXPECT_EQ(validate_lzss_red_black_tree(oversized),
              LzssRedBlackTreeValidationError::invalid_protocol_state);
}

TEST(LzssRedBlackTreeMatchFinder, AdvanceIndexesEverySkippedPosition) {
    const auto input = bytes("ABABABABABABABABXYZABABABAB");
    const auto required = calculate_lzss_red_black_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssRedBlackTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssRedBlackTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_red_black_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssRedBlackTreeError::none);
    LzssExhaustiveMatchFinder exhaustive{input, {}};

    const std::array<std::size_t, 4> landings{0, 2, 16, input.size()};
    for (std::size_t index = 0; index < landings.size(); ++index) {
        const auto position = landings[index];
        EXPECT_EQ(finder.find_match(position),
                  exhaustive.find_match(position)) << position;
        if (index + 1U != landings.size()) {
            const auto next = landings[index + 1U];
            finder.advance(position, next);
            exhaustive.advance(position, next);
            ASSERT_TRUE(finder.state_valid());
            ASSERT_EQ(validate_lzss_red_black_tree(finder),
                      LzssRedBlackTreeValidationError::none);
        }
    }
}

} // namespace

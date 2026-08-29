#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
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

[[nodiscard]] std::uint32_t exhaustive_maximum_lcp(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::size_t position) {
    const auto first = position > parameters.window_size
        ? position - parameters.window_size : 0;
    std::size_t best{};
    for (auto candidate = first; candidate < position; ++candidate) {
        const auto maximum = std::min({
            input.size() - position,
            input.size() - candidate,
            static_cast<std::size_t>(parameters.max_match_length)});
        std::size_t length{};
        while (length < maximum
               && input[position + length] == input[candidate + length]) {
            ++length;
        }
        best = std::max(best, length);
    }
    return static_cast<std::uint32_t>(best);
}

[[nodiscard]] LzssBinaryTreeCandidateQueryResult exhaustive_candidate(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::size_t position) {
    LzssBinaryTreeCandidateQueryResult result{};
    const auto first = position > parameters.window_size
        ? position - parameters.window_size : 0;
    for (auto candidate = first; candidate < position; ++candidate) {
        const auto maximum = std::min({
            input.size() - position,
            input.size() - candidate,
            static_cast<std::size_t>(parameters.max_match_length)});
        std::size_t length{};
        while (length < maximum
               && input[position + length] == input[candidate + length]) {
            ++length;
        }
        if (length > result.length
            || (length == result.length
                && candidate > result.candidate_position)) {
            result.candidate_position = candidate;
            result.length = static_cast<std::uint32_t>(length);
        }
    }
    if (result.length < parameters.min_match_length) {
        return {};
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

void expect_three_node_tree(
    const std::string_view text, const std::array<std::size_t, 3>& order,
    const std::size_t expected_root, const std::size_t expected_left,
    const std::size_t expected_right,
    const LzssParameters& parameters = {}) {
    const auto input = bytes(text);
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    for (const auto position : order) {
        ASSERT_EQ(insert_lzss_binary_tree_position(finder, position),
                  LzssBinaryTreeError::none);
        ASSERT_EQ(validate_lzss_binary_tree(finder),
                  LzssBinaryTreeValidationError::none);
    }

    ASSERT_EQ(finder.root_index(), expected_root);
    const auto root = inspect_lzss_binary_tree_node(
        finder, finder.root_index());
    EXPECT_EQ(root.left, expected_left);
    EXPECT_EQ(root.right, expected_right);
    EXPECT_EQ(root.parent, lzss_binary_tree_null_node);
    EXPECT_EQ(root.height, 2U);
    EXPECT_EQ(root.subtree_maximum_position, 16U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(
                  finder, static_cast<std::uint32_t>(expected_left)).parent,
              expected_root);
    EXPECT_EQ(inspect_lzss_binary_tree_node(
                  finder, static_cast<std::uint32_t>(expected_right)).parent,
              expected_root);
}

void expect_exact_finders_equal(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {}) {
    const auto binary_required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(binary_required.error, LzssBinaryTreeError::none);
    auto binary_storage = make_storage(binary_required.workspace_size);
    LzssBinaryTreeMatchFinder binary{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, binary_storage.bytes, binary),
              LzssBinaryTreeError::none);

    const auto hash_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(hash_required.error, LzssHashChainError::none);
    auto hash_storage = make_storage(hash_required.workspace_size);
    LzssHashChainMatchFinder hash{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {}, hash_storage.bytes, hash),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    for (std::size_t position = 0; position <= input.size(); ++position) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(binary.find_match(position), expected) << position;
        EXPECT_EQ(hash.find_match(position), expected) << position;
        if (position != input.size()) {
            binary.advance(position, position + 1U);
            hash.advance(position, position + 1U);
            exhaustive.advance(position, position + 1U);
            ASSERT_TRUE(binary.state_valid()) << position;
            ASSERT_EQ(validate_lzss_binary_tree(binary),
                      LzssBinaryTreeValidationError::none) << position;
        }
    }
}

TEST(LzssBinaryTreeMatchFinder, CalculatesSeparatedBoundedWorkspace) {
    auto required = calculate_lzss_binary_tree_workspace(4, {}, {});
    EXPECT_EQ(required.error, LzssBinaryTreeError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.node_count, 0U);

    required = calculate_lzss_binary_tree_workspace(5, {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    EXPECT_EQ(required.node_count, 5U);
    EXPECT_EQ(required.left_offset, 0U);
    EXPECT_EQ(required.right_offset, 20U);
    EXPECT_EQ(required.parent_offset, 40U);
    EXPECT_EQ(required.height_offset, 60U);
    const auto expected_position_offset = align_size(65U, alignof(std::size_t));
    const auto expected_subtree_offset =
        expected_position_offset + 5U * sizeof(std::size_t);
    EXPECT_EQ(required.position_offset, expected_position_offset);
    EXPECT_EQ(required.subtree_maximum_position_offset,
              expected_subtree_offset);
    EXPECT_EQ(required.workspace_size,
              expected_subtree_offset + 5U * sizeof(std::size_t));

    required = calculate_lzss_binary_tree_workspace(65'536, {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    EXPECT_EQ(required.node_count, 65'536U);
    EXPECT_EQ(required.workspace_alignment,
              std::max(alignof(std::size_t), alignof(std::uint32_t)));
    EXPECT_EQ(required.left_offset, 0U);
    EXPECT_EQ(required.right_offset,
              required.node_count * sizeof(std::uint32_t));
    EXPECT_EQ(required.parent_offset,
              required.right_offset
                  + required.node_count * sizeof(std::uint32_t));
    EXPECT_EQ(required.height_offset,
              required.parent_offset
                  + required.node_count * sizeof(std::uint32_t));
    EXPECT_EQ(required.position_offset,
              required.height_offset
                  + required.node_count * sizeof(std::uint8_t));
    EXPECT_EQ(required.subtree_maximum_position_offset,
              required.position_offset
                  + required.node_count * sizeof(std::size_t));
    EXPECT_EQ(required.workspace_size,
              required.node_count
                  * (3U * sizeof(std::uint32_t) + sizeof(std::uint8_t)
                     + 2U * sizeof(std::size_t)));

    LzssParameters large_window{};
    large_window.window_size = 1U << 20;
    required = calculate_lzss_binary_tree_workspace(
        1U << 20, large_window, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    EXPECT_EQ(required.node_count, 1U << 20);
    EXPECT_EQ(required.workspace_size,
              (1U << 20)
                  * (3U * sizeof(std::uint32_t) + sizeof(std::uint8_t)
                     + 2U * sizeof(std::size_t)));
}

TEST(LzssBinaryTreeMatchFinder, CalculatesSixteenMiBComparisonWorkspace) {
    if constexpr (sizeof(std::size_t) != 8U) GTEST_SKIP();

    constexpr std::size_t extent = 16U << 20;
    constexpr std::uint64_t policy = UINT64_C(512) << 20;
    LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(extent);
    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = policy;

    const auto hash = calculate_lzss_hash_chain_workspace(
        extent, parameters, limits);
    ASSERT_EQ(hash.error, LzssHashChainError::none);
    EXPECT_EQ(hash.workspace_size, 67'633'152U);
    EXPECT_EQ(extent + hash.workspace_size, 84'410'368U);

    const auto binary = calculate_lzss_binary_tree_workspace(
        extent, parameters, limits);
    ASSERT_EQ(binary.error, LzssBinaryTreeError::none);
    EXPECT_EQ(binary.workspace_size, 486'539'264U);
    EXPECT_EQ(extent + binary.workspace_size, 503'316'480U);
    EXPECT_EQ(policy - (extent + binary.workspace_size), 33'554'432U);

    limits.max_internal_buffered_bytes =
        static_cast<std::uint64_t>(extent + binary.workspace_size - 1U);
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(
                  extent, parameters, limits).error,
              LzssBinaryTreeError::workspace_limit_exceeded);
    limits.max_internal_buffered_bytes =
        static_cast<std::uint64_t>(extent + hash.workspace_size - 1U);
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(
                  extent, parameters, limits).error,
              LzssHashChainError::workspace_limit_exceeded);
}

TEST(LzssBinaryTreeMatchFinder, CalculatesSixtyFourMiBComparisonWorkspace) {
    if constexpr (sizeof(std::size_t) != 8U) GTEST_SKIP();

    constexpr std::size_t extent = 64U << 20;
    constexpr std::uint64_t policy = UINT64_C(2) << 30;
    LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(extent);
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = extent;
    limits.max_lz_distance = extent;
    limits.max_internal_buffered_bytes = policy;

    const auto hash = calculate_lzss_hash_chain_workspace(
        extent, parameters, limits);
    ASSERT_EQ(hash.error, LzssHashChainError::none);
    EXPECT_EQ(hash.workspace_size, 268'959'744U);
    EXPECT_EQ(extent + hash.workspace_size, 336'068'608U);

    const auto binary = calculate_lzss_binary_tree_workspace(
        extent, parameters, limits);
    ASSERT_EQ(binary.error, LzssBinaryTreeError::none);
    EXPECT_EQ(binary.workspace_size, 1'946'157'056U);
    EXPECT_EQ(extent + binary.workspace_size, 2'013'265'920U);
    EXPECT_EQ(policy - (extent + binary.workspace_size), 134'217'728U);

    limits.max_internal_buffered_bytes =
        static_cast<std::uint64_t>(extent + binary.workspace_size - 1U);
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(
                  extent, parameters, limits).error,
              LzssBinaryTreeError::workspace_limit_exceeded);
    limits.max_internal_buffered_bytes =
        static_cast<std::uint64_t>(extent + hash.workspace_size - 1U);
    EXPECT_EQ(calculate_lzss_hash_chain_workspace(
                  extent, parameters, limits).error,
              LzssHashChainError::workspace_limit_exceeded);
}

TEST(LzssBinaryTreeMatchFinder, InitializesEveryArrayAsEmpty) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size + 16U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), input.size());
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_binary_tree_null_node);

    const auto active = std::span<const std::byte>{storage.bytes}
        .first(required.workspace_size);
    for (const auto offset : {required.left_offset, required.right_offset,
                              required.parent_offset}) {
        const auto links = array_at<std::uint32_t>(
            active, offset, required.node_count);
        EXPECT_TRUE(std::ranges::all_of(links, [](const auto value) {
            return value == lzss_binary_tree_null_node;
        }));
    }
    const auto heights = array_at<std::uint8_t>(
        active, required.height_offset, required.node_count);
    EXPECT_TRUE(std::ranges::all_of(heights, [](const auto value) {
        return value == 0;
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

TEST(LzssBinaryTreeMatchFinder, InitializesShortInputWithoutWorkspace) {
    const auto input = bytes("ABCD");
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, {}, finder),
              LzssBinaryTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_EQ(finder.root_index(), lzss_binary_tree_null_node);
    finder.advance(0, input.size());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), input.size());
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, RejectsWorkspaceFailuresAtomically) {
    const auto seed_input = bytes("ABCD");
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  seed_input, {}, {}, {}, finder),
              LzssBinaryTreeError::none);

    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size + 1);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());
    EXPECT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1), finder),
              LzssBinaryTreeError::workspace_too_small);
    EXPECT_TRUE(finder.initialized());
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1, required.workspace_size), finder),
              LzssBinaryTreeError::misaligned_workspace);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    auto arena = make_storage(required.workspace_size + input.size());
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    const auto original_arena = std::vector<std::byte>(
        arena.bytes.begin(), arena.bytes.end());
    EXPECT_EQ(initialize_lzss_binary_tree_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssBinaryTreeError::overlapping_buffers);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_TRUE(std::ranges::equal(arena.bytes, original_arena));
}

TEST(LzssBinaryTreeMatchFinder, RejectsInvalidAndUnboundedRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(16, {}, limits).error,
              LzssBinaryTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_binary_tree_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssBinaryTreeError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(65, {}, limits).error,
              LzssBinaryTreeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 1U << 20;
    parameters = {};
    parameters.window_size = 65'536;
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(
                  65'536, parameters, limits).error,
              LzssBinaryTreeError::workspace_limit_exceeded);

    limits = {};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    parameters = {};
    EXPECT_EQ(calculate_lzss_binary_tree_workspace(
                  std::numeric_limits<std::size_t>::max(), parameters,
                  limits).error,
              LzssBinaryTreeError::arithmetic_overflow);
}

TEST(LzssBinaryTreeMatchFinder, PerformsDeterministicSingleRotations) {
    expect_three_node_tree(
        "C0000000B0000000A0000000", {0, 8, 16}, 8, 16, 0);
    expect_three_node_tree(
        "A0000000B0000000C0000000", {0, 8, 16}, 8, 0, 16);
}

TEST(LzssBinaryTreeMatchFinder, PerformsDeterministicDoubleRotations) {
    expect_three_node_tree(
        "C0000000A0000000B0000000", {0, 8, 16}, 16, 8, 0);
    expect_three_node_tree(
        "A0000000C0000000B0000000", {0, 8, 16}, 16, 0, 8);
}

TEST(LzssBinaryTreeMatchFinder, OrdersEqualCappedSuffixByPosition) {
    LzssParameters parameters{};
    parameters.max_match_length = 5;
    expect_three_node_tree(
        "ABCDE___ABCDE___ABCDE___", {0, 8, 16}, 8, 0, 16,
        parameters);
}

TEST(LzssBinaryTreeMatchFinder, RejectsInvalidInsertionAtomically) {
    LzssBinaryTreeMatchFinder uninitialized{};
    EXPECT_EQ(insert_lzss_binary_tree_position(uninitialized, 0),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(validate_lzss_binary_tree(uninitialized),
              LzssBinaryTreeValidationError::uninitialized);

    const auto short_input = bytes("ABCD");
    LzssBinaryTreeMatchFinder short_finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  short_input, {}, {}, {}, short_finder),
              LzssBinaryTreeError::none);
    EXPECT_EQ(insert_lzss_binary_tree_position(short_finder, 0),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(validate_lzss_binary_tree(short_finder),
              LzssBinaryTreeValidationError::none);

    const auto input = bytes("ABCDE___FGHIJ___KLMNO___");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    ASSERT_EQ(insert_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::none);
    const auto root_before = inspect_lzss_binary_tree_node(
        finder, finder.root_index());
    EXPECT_EQ(insert_lzss_binary_tree_position(finder, input.size() - 4U),
              LzssBinaryTreeError::invalid_position);
    EXPECT_EQ(insert_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(insert_lzss_binary_tree_position(finder, 8),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, finder.root_index()).position,
              root_before.position);
    EXPECT_EQ(finder.active_node_count(), 1U);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, ValidatorDetectsMetadataAndLinkCorruption) {
    const auto input = bytes("A0000000B0000000C0000000");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    for (const auto position : {0U, 8U, 16U}) {
        ASSERT_EQ(insert_lzss_binary_tree_position(finder, position),
                  LzssBinaryTreeError::none);
    }
    ASSERT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);

    auto links = mutable_array_at<std::uint32_t>(
        storage.bytes, required.left_offset, required.node_count);
    auto parents = mutable_array_at<std::uint32_t>(
        storage.bytes, required.parent_offset, required.node_count);
    auto heights = mutable_array_at<std::uint8_t>(
        storage.bytes, required.height_offset, required.node_count);
    auto subtree_maximum = mutable_array_at<std::size_t>(
        storage.bytes, required.subtree_maximum_position_offset,
        required.node_count);
    auto positions = mutable_array_at<std::size_t>(
        storage.bytes, required.position_offset, required.node_count);
    const auto root = finder.root_index();
    const auto left = links[root];
    const auto right = inspect_lzss_binary_tree_node(finder, root).right;

    heights[root] = 3;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_height);
    heights[root] = 2;

    subtree_maximum[root] = 8;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_subtree_maximum);
    subtree_maximum[root] = 16;

    const auto left_position = positions[left];
    positions[left] = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_slot_position);
    positions[left] = left_position;

    parents[left] = right;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_parent);
    parents[left] = root;

    parents[left] = left;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::cycle_or_disconnected);
    parents[left] = root;

    links[root] = static_cast<std::uint32_t>(required.node_count);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_index);
    links[root] = left;

    links[1] = root;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_inactive_node);
    links[1] = lzss_binary_tree_null_node;
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, BulkInsertionIsBalancedAndDeterministic) {
    std::vector<std::byte> input(512);
    std::uint32_t state = UINT32_C(0x13579bdf);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto first_storage = make_storage(required.workspace_size);
    auto second_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder first{};
    LzssBinaryTreeMatchFinder second{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, first_storage.bytes, first),
              LzssBinaryTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssBinaryTreeError::none);
    const auto indexable_count = input.size() - lzss_binary_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < indexable_count; ++position) {
        ASSERT_EQ(insert_lzss_binary_tree_position(first, position),
                  LzssBinaryTreeError::none);
        ASSERT_EQ(insert_lzss_binary_tree_position(second, position),
                  LzssBinaryTreeError::none);
    }

    EXPECT_EQ(validate_lzss_binary_tree(first),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(validate_lzss_binary_tree(second),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(first.active_node_count(), indexable_count);
    ASSERT_NE(first.root_index(), lzss_binary_tree_null_node);
    const auto root = inspect_lzss_binary_tree_node(first, first.root_index());
    EXPECT_LE(root.height, 2U * std::bit_width(indexable_count));
    EXPECT_EQ(root.subtree_maximum_position, indexable_count - 1U);
    EXPECT_EQ(first.root_index(), second.root_index());
    for (std::size_t node = 0; node < input.size(); ++node) {
        EXPECT_EQ(inspect_lzss_binary_tree_node(
                      first, static_cast<std::uint32_t>(node)),
                  inspect_lzss_binary_tree_node(
                      second, static_cast<std::uint32_t>(node)));
    }
}

TEST(LzssBinaryTreeMatchFinder, RemovesLeafRebalancesAndRemovesRootStructurally) {
    const auto input = bytes("A0000000B0000000C0000000D0000000");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U}) {
        ASSERT_EQ(insert_lzss_binary_tree_position(finder, position),
                  LzssBinaryTreeError::none);
    }
    ASSERT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::none);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 0).height, 0U);
    EXPECT_EQ(finder.root_index(), 16U);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 16),
              LzssBinaryTreeError::none);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    ASSERT_EQ(finder.root_index(), 24U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 24).left, 8U);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 24),
              LzssBinaryTreeError::none);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 8U);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 8),
              LzssBinaryTreeError::none);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(finder.root_index(), lzss_binary_tree_null_node);
}

TEST(LzssBinaryTreeMatchFinder, TransplantsDirectRootSuccessor) {
    const auto input = bytes("A0000000B0000000C0000000");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    for (const auto position : {0U, 8U, 16U}) {
        ASSERT_EQ(insert_lzss_binary_tree_position(finder, position),
                  LzssBinaryTreeError::none);
    }
    ASSERT_EQ(finder.root_index(), 8U);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 8),
              LzssBinaryTreeError::none);
    ASSERT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 16U);
    const auto root = inspect_lzss_binary_tree_node(finder, 16);
    EXPECT_EQ(root.left, 0U);
    EXPECT_EQ(root.right, lzss_binary_tree_null_node);
    EXPECT_EQ(root.subtree_maximum_position, 16U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 8).height, 0U);
}

TEST(LzssBinaryTreeMatchFinder, TransplantsNonDirectSuccessorWithoutPayloadSwap) {
    const auto input = bytes(
        "D0000000B0000000F0000000E0000000G0000000");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    for (const auto position : {0U, 8U, 16U, 24U, 32U}) {
        ASSERT_EQ(insert_lzss_binary_tree_position(finder, position),
                  LzssBinaryTreeError::none);
    }
    ASSERT_EQ(finder.root_index(), 0U);

    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::none);
    ASSERT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 24U);
    const auto root = inspect_lzss_binary_tree_node(finder, 24);
    EXPECT_EQ(root.position, 24U);
    EXPECT_EQ(root.left, 8U);
    EXPECT_EQ(root.right, 16U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 16).right, 32U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 0).height, 0U);
}

TEST(LzssBinaryTreeMatchFinder, RemovedSlotCanBeReusedByLaterPosition) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    ASSERT_EQ(insert_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::none);
    ASSERT_EQ(remove_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::none);
    ASSERT_EQ(insert_lzss_binary_tree_position(finder, 8),
              LzssBinaryTreeError::none);
    ASSERT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(finder.root_index(), 0U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 0).position, 8U);
}

TEST(LzssBinaryTreeMatchFinder, RejectsInvalidRemovalAtomically) {
    LzssBinaryTreeMatchFinder uninitialized{};
    EXPECT_EQ(remove_lzss_binary_tree_position(uninitialized, 0),
              LzssBinaryTreeError::invalid_state);

    const auto input = bytes("A0000000B0000000C0000000");
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    ASSERT_EQ(insert_lzss_binary_tree_position(finder, 8),
              LzssBinaryTreeError::none);
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());
    const auto root = finder.root_index();

    EXPECT_EQ(remove_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(remove_lzss_binary_tree_position(finder, input.size() - 4U),
              LzssBinaryTreeError::invalid_position);
    EXPECT_EQ(finder.root_index(), root);
    EXPECT_EQ(finder.active_node_count(), 1U);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, BulkDeletionRemainsBalancedAndDeterministic) {
    std::vector<std::byte> input(256);
    std::uint32_t state = UINT32_C(0x2468ace1);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto first_storage = make_storage(required.workspace_size);
    auto second_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder first{};
    LzssBinaryTreeMatchFinder second{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, first_storage.bytes, first),
              LzssBinaryTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssBinaryTreeError::none);
    const auto count = input.size() - lzss_binary_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_binary_tree_position(first, position),
                  LzssBinaryTreeError::none);
        ASSERT_EQ(insert_lzss_binary_tree_position(second, position),
                  LzssBinaryTreeError::none);
    }

    for (std::size_t parity = 0; parity < 2; ++parity) {
        for (std::size_t position = parity; position < count; position += 2) {
            ASSERT_EQ(remove_lzss_binary_tree_position(first, position),
                      LzssBinaryTreeError::none) << position;
            ASSERT_EQ(remove_lzss_binary_tree_position(second, position),
                      LzssBinaryTreeError::none) << position;
            ASSERT_EQ(validate_lzss_binary_tree(first),
                      LzssBinaryTreeValidationError::none) << position;
            ASSERT_EQ(validate_lzss_binary_tree(second),
                      LzssBinaryTreeValidationError::none) << position;
            EXPECT_EQ(first.root_index(), second.root_index());
        }
        for (std::size_t node = 0; node < input.size(); ++node) {
            EXPECT_EQ(inspect_lzss_binary_tree_node(
                          first, static_cast<std::uint32_t>(node)),
                      inspect_lzss_binary_tree_node(
                          second, static_cast<std::uint32_t>(node)));
        }
    }
    EXPECT_TRUE(first.empty());
    EXPECT_TRUE(second.empty());
}

TEST(LzssBinaryTreeMatchFinder, AdvancesAfterQueryAndRetainsWindowBoundary) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    finder.advance(0, 8);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), 8U);
    EXPECT_EQ(finder.active_node_count(), 8U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 0).position, 0U);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);

    finder.advance(8, 9);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), 9U);
    EXPECT_EQ(finder.active_node_count(), 8U);
    EXPECT_EQ(inspect_lzss_binary_tree_node(finder, 0).position, 8U);
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, AdvancesEverySkippedPositionSequentially) {
    std::vector<std::byte> input(32);
    for (std::size_t position = 0; position < input.size(); ++position) {
        input[position] = static_cast<std::byte>(position);
    }
    LzssParameters parameters{};
    parameters.window_size = 32;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    finder.advance(0, 16);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), 16U);
    EXPECT_EQ(finder.active_node_count(), 16U);
    for (std::size_t position = 0; position < 16; ++position) {
        EXPECT_EQ(inspect_lzss_binary_tree_node(
                      finder, static_cast<std::uint32_t>(position)).position,
                  position);
    }
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, AdvancesThroughNonIndexableInputTail) {
    const auto input = bytes("ABCDEFGHIJKL");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    finder.advance(0, input.size());
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), input.size());
    EXPECT_EQ(finder.active_node_count(), 4U);
    for (std::size_t position = 0; position < 4; ++position) {
        EXPECT_EQ(inspect_lzss_binary_tree_node(
                      finder, static_cast<std::uint32_t>(position)).height,
                  0U);
    }
    for (std::size_t position = 4; position < 8; ++position) {
        EXPECT_EQ(inspect_lzss_binary_tree_node(
                      finder, static_cast<std::uint32_t>(position)).position,
                  position);
    }
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, AdvanceIsIndependentOfCallerChunking) {
    std::vector<std::byte> input(128);
    std::uint32_t state = UINT32_C(0x5a17c3e9);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto bulk_storage = make_storage(required.workspace_size);
    auto byte_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder bulk{};
    LzssBinaryTreeMatchFinder bytewise{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, bulk_storage.bytes, bulk),
              LzssBinaryTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, byte_storage.bytes, bytewise),
              LzssBinaryTreeError::none);

    bulk.advance(0, input.size());
    for (std::size_t position = 0; position < input.size(); ++position) {
        bytewise.advance(position, position + 1U);
    }

    ASSERT_TRUE(bulk.state_valid());
    ASSERT_TRUE(bytewise.state_valid());
    EXPECT_EQ(validate_lzss_binary_tree(bulk),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(validate_lzss_binary_tree(bytewise),
              LzssBinaryTreeValidationError::none);
    EXPECT_EQ(bulk.next_position(), bytewise.next_position());
    EXPECT_EQ(bulk.active_node_count(), bytewise.active_node_count());
    EXPECT_EQ(bulk.root_index(), bytewise.root_index());
    for (std::size_t node = 0; node < input.size(); ++node) {
        EXPECT_EQ(inspect_lzss_binary_tree_node(
                      bulk, static_cast<std::uint32_t>(node)),
                  inspect_lzss_binary_tree_node(
                      bytewise, static_cast<std::uint32_t>(node)));
    }
}

TEST(LzssBinaryTreeMatchFinder, InvalidAdvanceOrderIsSticky) {
    const auto input = bytes("A0000000B0000000C0000000");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    finder.advance(1, 2);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.next_position(), input.size());
    EXPECT_EQ(finder.active_node_count(), 0U);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_protocol_state);
    EXPECT_EQ(insert_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(remove_lzss_binary_tree_position(finder, 0),
              LzssBinaryTreeError::invalid_state);

    finder.advance(input.size(), input.size());
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::invalid_protocol_state);

    auto backward_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder backward{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, backward_storage.bytes, backward),
              LzssBinaryTreeError::none);
    backward.advance(0, 4);
    ASSERT_TRUE(backward.state_valid());
    backward.advance(4, 3);
    EXPECT_FALSE(backward.state_valid());
    EXPECT_EQ(validate_lzss_binary_tree(backward),
              LzssBinaryTreeValidationError::invalid_protocol_state);

    auto oversized_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder oversized{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, oversized_storage.bytes, oversized),
              LzssBinaryTreeError::none);
    oversized.advance(0, input.size() + 1U);
    EXPECT_FALSE(oversized.state_valid());
    EXPECT_EQ(validate_lzss_binary_tree(oversized),
              LzssBinaryTreeValidationError::invalid_protocol_state);
}

TEST(LzssBinaryTreeMatchFinder, FindsBothLexicographicNeighborsAndTheirLcp) {
    const auto input = bytes("ABCDE___ABCDG___ABCDF___");
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    finder.advance(0, 16);
    ASSERT_TRUE(finder.state_valid());

    const auto result = finder.find_neighbors(16);
    EXPECT_EQ(result.error, LzssBinaryTreeError::none);
    EXPECT_EQ(result.predecessor_position, 0U);
    EXPECT_EQ(result.successor_position, 8U);
    EXPECT_EQ(result.predecessor_lcp, 4U);
    EXPECT_EQ(result.successor_lcp, 4U);
    EXPECT_EQ(result.maximum_lcp, 4U);
    EXPECT_EQ(finder.find_candidate(16),
              LzssBinaryTreeCandidateQueryResult{});
    EXPECT_EQ(validate_lzss_binary_tree(finder),
              LzssBinaryTreeValidationError::none);
}

TEST(LzssBinaryTreeMatchFinder, VirtualQueryFollowsEqualCappedSuffixes) {
    const auto input = bytes("ABCDE___ABCDE___ABCDE___");
    LzssParameters parameters{};
    parameters.window_size = 16;
    parameters.max_match_length = 5;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    finder.advance(0, 16);

    const auto result = finder.find_neighbors(16);
    EXPECT_EQ(result.error, LzssBinaryTreeError::none);
    EXPECT_EQ(result.predecessor_position, 8U);
    EXPECT_EQ(result.predecessor_lcp, 5U);
    EXPECT_EQ(result.maximum_lcp, 5U);
}

TEST(LzssBinaryTreeMatchFinder, NeighborQueryHandlesEmptyTailAndInvalidState) {
    LzssBinaryTreeMatchFinder uninitialized{};
    EXPECT_EQ(uninitialized.find_neighbors(0).error,
              LzssBinaryTreeError::invalid_state);

    const auto input = bytes("ABCDEFGHIJKL");
    LzssParameters parameters{};
    parameters.window_size = 8;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);

    auto empty_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder empty{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, empty_storage.bytes, empty),
              LzssBinaryTreeError::none);
    EXPECT_EQ(empty.find_neighbors(0), LzssBinaryTreeNeighborQueryResult{});
    EXPECT_EQ(empty.find_neighbors(1).error,
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(empty.find_neighbors(input.size() + 1U).error,
              LzssBinaryTreeError::invalid_position);

    auto tail_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder tail{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, tail_storage.bytes, tail),
              LzssBinaryTreeError::none);
    tail.advance(0, input.size() - 4U);
    EXPECT_EQ(tail.find_neighbors(input.size() - 4U),
              LzssBinaryTreeNeighborQueryResult{});
    tail.advance(input.size() - 4U, input.size());
    EXPECT_EQ(tail.find_neighbors(input.size()),
              LzssBinaryTreeNeighborQueryResult{});

    auto invalid_storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder invalid{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, invalid_storage.bytes, invalid),
              LzssBinaryTreeError::none);
    invalid.advance(1, 2);
    EXPECT_EQ(invalid.find_neighbors(input.size()).error,
              LzssBinaryTreeError::invalid_state);
    EXPECT_EQ(invalid.find_candidate(input.size()).error,
              LzssBinaryTreeError::invalid_state);
}

TEST(LzssBinaryTreeMatchFinder, NeighborMaximumLcpMatchesWindowEnumeration) {
    std::vector<std::byte> input(257);
    std::uint32_t state = UINT32_C(0x7915b3cd);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 27U);
    }
    LzssParameters parameters{};
    parameters.window_size = 32;
    parameters.max_match_length = 64;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    const auto last_query = input.size() - lzss_binary_tree_prefix_size;
    for (std::size_t position = 0; position <= last_query; ++position) {
        const auto result = finder.find_neighbors(position);
        ASSERT_EQ(result.error, LzssBinaryTreeError::none) << position;
        EXPECT_EQ(result.maximum_lcp,
                  exhaustive_maximum_lcp(input, parameters, position))
            << position;
        const auto first = position > parameters.window_size
            ? position - parameters.window_size : 0;
        for (const auto neighbor : {result.predecessor_position,
                                    result.successor_position}) {
            if (neighbor != lzss_binary_tree_no_position) {
                EXPECT_GE(neighbor, first) << position;
                EXPECT_LT(neighbor, position) << position;
            }
        }
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
        ASSERT_EQ(validate_lzss_binary_tree(finder),
                  LzssBinaryTreeValidationError::none) << position;
    }
}

TEST(LzssBinaryTreeMatchFinder, PrefixRangeSelectsNonadjacentNewestCandidate) {
    const auto input = bytes(
        "ABCDEA__ABCDEL__ABCDEN__ABCDEB__ABCDEM__");
    LzssParameters parameters{};
    parameters.window_size = 32;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    finder.advance(0, 32);

    const auto neighbors = finder.find_neighbors(32);
    ASSERT_EQ(neighbors.error, LzssBinaryTreeError::none);
    EXPECT_EQ(neighbors.predecessor_position, 8U);
    EXPECT_EQ(neighbors.successor_position, 16U);
    EXPECT_EQ(neighbors.maximum_lcp, 5U);
    const auto candidate = finder.find_candidate(32);
    EXPECT_EQ(candidate.error, LzssBinaryTreeError::none);
    EXPECT_EQ(candidate.candidate_position, 24U);
    EXPECT_EQ(candidate.length, 5U);
    EXPECT_EQ(finder.find_match(32), (LzssMatch{8, 5}));
}

TEST(LzssBinaryTreeMatchFinder, PrefixRangeHandlesAllMaximumBytePrefix) {
    std::vector<std::byte> input(24, std::byte{0});
    for (const auto position : {0U, 8U, 16U}) {
        std::ranges::fill(
            std::span{input}.subspan(position, 5), std::byte{0xff});
    }
    input[5] = std::byte{2};
    input[13] = std::byte{1};
    input[21] = std::byte{3};
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);
    finder.advance(0, 16);

    const auto neighbors = finder.find_neighbors(16);
    ASSERT_EQ(neighbors.error, LzssBinaryTreeError::none);
    EXPECT_EQ(neighbors.predecessor_position, 0U);
    EXPECT_EQ(neighbors.successor_position, lzss_binary_tree_no_position);
    EXPECT_EQ(neighbors.maximum_lcp, 5U);
    const auto candidate = finder.find_candidate(16);
    EXPECT_EQ(candidate.error, LzssBinaryTreeError::none);
    EXPECT_EQ(candidate.candidate_position, 8U);
    EXPECT_EQ(candidate.length, 5U);
}

TEST(LzssBinaryTreeMatchFinder, CandidateMatchesActiveWindowEnumeration) {
    std::vector<std::byte> input(512);
    std::uint32_t state = UINT32_C(0xc31d7a59);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 30U);
    }
    LzssParameters parameters{};
    parameters.window_size = 64;
    parameters.max_match_length = 64;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder),
              LzssBinaryTreeError::none);

    std::size_t qualifying_queries{};
    const auto last_query = input.size() - lzss_binary_tree_prefix_size;
    for (std::size_t position = 0; position <= last_query; ++position) {
        const auto expected = exhaustive_candidate(input, parameters, position);
        const auto actual = finder.find_candidate(position);
        ASSERT_EQ(actual.error, LzssBinaryTreeError::none) << position;
        EXPECT_EQ(actual, expected) << position;
        if (actual.candidate_position != lzss_binary_tree_no_position) {
            ++qualifying_queries;
        }
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
        ASSERT_EQ(validate_lzss_binary_tree(finder),
                  LzssBinaryTreeValidationError::none) << position;
    }
    EXPECT_GT(qualifying_queries, 0U);
}

TEST(LzssBinaryTreeMatchFinder, ExactMatchEqualsOtherFindersAcrossInputClasses) {
    expect_exact_finders_equal(bytes(""));
    expect_exact_finders_equal(bytes("A"));
    expect_exact_finders_equal(bytes("ABABABABABABABAB"));
    expect_exact_finders_equal(bytes("ABCDE1ABCDE2ABCDE3"));
    expect_exact_finders_equal(bytes(
        "AAAAABAAAACAAAAADAAAAEAAAAAFAAAAAGAAAAAHAAAAAI"));

    std::vector<std::byte> all_values{};
    for (std::uint32_t value = 0; value < 256; ++value) {
        all_values.push_back(static_cast<std::byte>(value));
    }
    all_values.insert(all_values.end(), all_values.begin(), all_values.end());
    expect_exact_finders_equal(all_values);

    std::vector<std::byte> pseudorandom(1024);
    std::uint32_t state = UINT32_C(0x13579bdf);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_exact_finders_equal(pseudorandom);
}

TEST(LzssBinaryTreeMatchFinder, ExactMatchEqualsParameterMatrix) {
    std::vector<std::byte> input{};
    for (std::size_t index = 0; index < 512; ++index) {
        input.push_back(static_cast<std::byte>(
            index % 29 == 0 ? index & 0xffU : index % 7));
    }
    for (const std::uint32_t window : {1U, 5U, 17U, 256U, 65'536U}) {
        for (const std::uint32_t maximum : {5U, 17U, 258U}) {
            LzssParameters parameters{};
            parameters.window_size = window;
            parameters.max_match_length = maximum;
            expect_exact_finders_equal(input, parameters);
        }
    }
}

TEST(LzssBinaryTreeMatchFinder, ExactMatchIndexesEverySkippedPosition) {
    const auto input = bytes("ABABABABABABABABXYZABABABAB");
    const auto binary_required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(binary_required.error, LzssBinaryTreeError::none);
    auto binary_storage = make_storage(binary_required.workspace_size);
    LzssBinaryTreeMatchFinder binary{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, binary_storage.bytes, binary),
              LzssBinaryTreeError::none);
    const auto hash_required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    ASSERT_EQ(hash_required.error, LzssHashChainError::none);
    auto hash_storage = make_storage(hash_required.workspace_size);
    LzssHashChainMatchFinder hash{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, hash_storage.bytes, hash),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, {}};

    const std::array<std::size_t, 4> landings{0, 2, 16, input.size()};
    for (std::size_t index = 0; index < landings.size(); ++index) {
        const auto position = landings[index];
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(binary.find_match(position), expected) << position;
        EXPECT_EQ(hash.find_match(position), expected) << position;
        if (index + 1U != landings.size()) {
            const auto next = landings[index + 1U];
            binary.advance(position, next);
            hash.advance(position, next);
            exhaustive.advance(position, next);
            ASSERT_TRUE(binary.state_valid());
            ASSERT_EQ(validate_lzss_binary_tree(binary),
                      LzssBinaryTreeValidationError::none);
        }
    }
}

TEST(LzssBinaryTreeMatchFinder, ReportsOptionalBoundedWorkStatistics) {
    const auto input = bytes(
        "ABCDE1ABCDE2ABCDE3ABCDE4ABCDE5ABCDE6ABCDE7ABCDE8");
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(required.error, LzssBinaryTreeError::none);
    auto measured_storage = make_storage(required.workspace_size);
    auto plain_storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssBinaryTreeMatchFinder measured{};
    LzssBinaryTreeMatchFinder plain{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, measured_storage.bytes, measured,
                  &statistics),
              LzssBinaryTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, plain_storage.bytes, plain),
              LzssBinaryTreeError::none);

    for (std::size_t position = 0; position <= input.size(); ++position) {
        EXPECT_EQ(measured.find_match(position), plain.find_match(position))
            << position;
        if (position != input.size()) {
            measured.advance(position, position + 1U);
            plain.advance(position, position + 1U);
            ASSERT_TRUE(measured.state_valid()) << position;
            ASSERT_TRUE(plain.state_valid()) << position;
        }
    }

    EXPECT_EQ(statistics.query_count, input.size() + 1U);
    EXPECT_EQ(statistics.binary_tree_insertion_count, input.size() - 4U);
    EXPECT_EQ(statistics.binary_tree_retirement_count,
              input.size() - parameters.window_size);
    EXPECT_GT(statistics.binary_tree_key_comparison_count, 0U);
    EXPECT_GT(statistics.binary_tree_key_byte_comparison_count, 0U);
    EXPECT_GT(statistics.binary_tree_lcp_byte_comparison_count, 0U);
    EXPECT_GT(statistics.binary_tree_prefix_range_comparison_count, 0U);
    EXPECT_GT(statistics.binary_tree_rotation_count, 0U);
    EXPECT_GT(statistics.binary_tree_maximum_height, 0U);
    EXPECT_GT(statistics.binary_tree_maximum_nodes_per_query, 0U);
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.binary_tree_query_depth_histogram) {
        histogram_queries += count;
    }
    EXPECT_EQ(histogram_queries, statistics.query_count);
    EXPECT_FALSE(statistics.overflowed);
}

TEST(LzssBinaryTreeMatchFinder, ReportsStatisticsCounterOverflow) {
    const std::span<const std::byte> input{};
    LzssMatchFinderStatistics statistics{};
    statistics.query_count = std::numeric_limits<std::uint64_t>::max();
    statistics.binary_tree_query_depth_histogram[0] =
        std::numeric_limits<std::uint64_t>::max();
    LzssBinaryTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, {}, finder, &statistics),
              LzssBinaryTreeError::none);

    EXPECT_EQ(finder.find_match(0), LzssMatch{});
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.query_count,
              std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(statistics.binary_tree_query_depth_histogram[0],
              std::numeric_limits<std::uint64_t>::max());
}

} // namespace

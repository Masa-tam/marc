#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_wavl_tree_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
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

struct InitializedWavl {
    std::vector<std::byte> input{};
    LzssWavlTreeWorkspaceRequirements required{};
    AlignedStorage storage{};
    std::unique_ptr<LzssMatchFinderStatistics> statistics{
        std::make_unique<LzssMatchFinderStatistics>()};
    LzssWavlTreeMatchFinder finder{};
};

[[nodiscard]] InitializedWavl initialize_wavl(const std::string_view text) {
    InitializedWavl result{};
    result.input = bytes(text);
    result.required = calculate_lzss_wavl_tree_workspace(
        result.input.size(), {}, {});
    EXPECT_EQ(result.required.error, LzssWavlTreeError::none);
    result.storage = make_storage(result.required.workspace_size);
    EXPECT_EQ(initialize_lzss_wavl_tree_match_finder(
                  result.input, {}, {}, result.storage.bytes,
                  result.finder, result.statistics.get()),
              LzssWavlTreeError::none);
    return result;
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

void expect_wavl_queries_equal_avl(
    const std::vector<std::byte>& input,
    const LzssParameters& parameters) {
    const auto wavl_required = calculate_lzss_wavl_tree_workspace(
        input.size(), parameters, {});
    const auto avl_required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(wavl_required.error, LzssWavlTreeError::none);
    ASSERT_EQ(avl_required.error, LzssBinaryTreeError::none);
    auto wavl_storage = make_storage(wavl_required.workspace_size);
    auto avl_storage = make_storage(avl_required.workspace_size);
    LzssWavlTreeMatchFinder wavl{};
    LzssBinaryTreeMatchFinder avl{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, parameters, {}, wavl_storage.bytes, wavl),
              LzssWavlTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, avl_storage.bytes, avl),
              LzssBinaryTreeError::none);

    for (std::size_t position = 0; position <= input.size(); ++position) {
        SCOPED_TRACE(position);
        const auto wavl_neighbors = wavl.find_neighbors(position);
        const auto avl_neighbors = avl.find_neighbors(position);
        EXPECT_EQ(wavl_neighbors.error, LzssWavlTreeError::none);
        EXPECT_EQ(avl_neighbors.error, LzssBinaryTreeError::none);
        EXPECT_EQ(wavl_neighbors.predecessor_position,
                  avl_neighbors.predecessor_position);
        EXPECT_EQ(wavl_neighbors.successor_position,
                  avl_neighbors.successor_position);
        EXPECT_EQ(wavl_neighbors.predecessor_lcp,
                  avl_neighbors.predecessor_lcp);
        EXPECT_EQ(wavl_neighbors.successor_lcp,
                  avl_neighbors.successor_lcp);
        EXPECT_EQ(wavl_neighbors.maximum_lcp,
                  avl_neighbors.maximum_lcp);

        const auto wavl_candidate = wavl.find_candidate(position);
        const auto avl_candidate = avl.find_candidate(position);
        EXPECT_EQ(wavl_candidate.error, LzssWavlTreeError::none);
        EXPECT_EQ(avl_candidate.error, LzssBinaryTreeError::none);
        EXPECT_EQ(wavl_candidate.candidate_position,
                  avl_candidate.candidate_position);
        EXPECT_EQ(wavl_candidate.length, avl_candidate.length);
        EXPECT_EQ(wavl.find_match(position), avl.find_match(position));

        if (position == input.size()) continue;
        wavl.advance(position, position + 1U);
        avl.advance(position, position + 1U);
        ASSERT_TRUE(wavl.state_valid());
        ASSERT_TRUE(avl.state_valid());
        ASSERT_EQ(validate_lzss_wavl_tree(wavl),
                  LzssWavlTreeValidationError::none);
        ASSERT_EQ(validate_lzss_binary_tree(avl),
                  LzssBinaryTreeValidationError::none);
        EXPECT_EQ(wavl.active_node_count(), avl.active_node_count());
        EXPECT_EQ(wavl.next_position(), avl.next_position());
    }
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

TEST(LzssWavlTreeMatchFinder, CheckedRankPromotionPreservesFailureOutput) {
    std::uint8_t promoted = 0xa5;
    EXPECT_EQ(calculate_lzss_wavl_promoted_rank(253, promoted),
              LzssWavlTreeError::none);
    EXPECT_EQ(promoted, 254U);

    promoted = 0xa5;
    EXPECT_EQ(calculate_lzss_wavl_promoted_rank(254, promoted),
              LzssWavlTreeError::rank_overflow);
    EXPECT_EQ(promoted, 0xa5U);

    EXPECT_EQ(calculate_lzss_wavl_promoted_rank(
                  lzss_wavl_tree_inactive_rank, promoted),
              LzssWavlTreeError::invalid_state);
    EXPECT_EQ(promoted, 0xa5U);
}

TEST(LzssWavlTreeMatchFinder, InsertsWithI0AndI1Promotions) {
    auto fixture = initialize_wavl("CADxxxxx");
    ASSERT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);

    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::none);
    EXPECT_EQ(fixture.finder.root_index(), 0U);
    EXPECT_EQ(inspect_lzss_wavl_tree_node(fixture.finder, 0).rank, 0U);

    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 1),
              LzssWavlTreeError::none);
    EXPECT_EQ(inspect_lzss_wavl_tree_node(fixture.finder, 0).rank, 1U);
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_promotion_count, 1U);

    // D attaches to the root's absent right edge. Its rank difference is
    // already one, so I0 performs no rank change or rotation.
    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 2),
              LzssWavlTreeError::none);
    EXPECT_EQ(fixture.finder.root_index(), 0U);
    EXPECT_EQ(inspect_lzss_wavl_tree_node(fixture.finder, 0).rank, 1U);
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_promotion_count, 1U);
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_single_rotation_count,
              0U);
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_double_rotation_count,
              0U);
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_fixup_step_count, 1U);
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);
}

TEST(LzssWavlTreeMatchFinder, AppliesMirroredI2SingleRotations) {
    for (const auto text : {"CBAxxxxx", "ABCxxxxx"}) {
        auto fixture = initialize_wavl(text);
        for (const auto position : {0U, 1U, 2U}) {
            ASSERT_EQ(insert_lzss_wavl_tree_position(
                          fixture.finder, position),
                      LzssWavlTreeError::none) << text;
            ASSERT_EQ(validate_lzss_wavl_tree(fixture.finder),
                      LzssWavlTreeValidationError::none) << text;
        }
        ASSERT_EQ(fixture.finder.root_index(), 1U) << text;
        const auto root = inspect_lzss_wavl_tree_node(fixture.finder, 1);
        EXPECT_EQ(root.rank, 1U) << text;
        EXPECT_EQ(root.left, text[0] == 'C' ? 2U : 0U) << text;
        EXPECT_EQ(root.right, text[0] == 'C' ? 0U : 2U) << text;
        EXPECT_EQ(root.subtree_maximum_position, 2U) << text;
        EXPECT_EQ(fixture.statistics->wavl_tree_insertion_promotion_count, 2U);
        EXPECT_EQ(
            fixture.statistics->wavl_tree_insertion_single_rotation_count,
            1U);
        EXPECT_EQ(
            fixture.statistics->wavl_tree_insertion_double_rotation_count,
            0U);
        EXPECT_EQ(fixture.statistics->wavl_tree_insertion_fixup_step_count, 3U);
        EXPECT_EQ(fixture.statistics->wavl_tree_maximum_insertion_fixup_steps,
                  2U);
        EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
                  LzssWavlTreeValidationError::none) << text;
    }
}

TEST(LzssWavlTreeMatchFinder, AppliesMirroredI3DoubleRotations) {
    for (const auto text : {"CABxxxxx", "ACBxxxxx"}) {
        auto fixture = initialize_wavl(text);
        for (const auto position : {0U, 1U, 2U}) {
            ASSERT_EQ(insert_lzss_wavl_tree_position(
                          fixture.finder, position),
                      LzssWavlTreeError::none) << text;
            ASSERT_EQ(validate_lzss_wavl_tree(fixture.finder),
                      LzssWavlTreeValidationError::none) << text;
        }
        ASSERT_EQ(fixture.finder.root_index(), 2U) << text;
        const auto root = inspect_lzss_wavl_tree_node(fixture.finder, 2);
        EXPECT_EQ(root.rank, 1U) << text;
        EXPECT_EQ(root.left, text[0] == 'C' ? 1U : 0U) << text;
        EXPECT_EQ(root.right, text[0] == 'C' ? 0U : 1U) << text;
        EXPECT_EQ(root.subtree_maximum_position, 2U) << text;
        EXPECT_EQ(fixture.statistics->wavl_tree_insertion_promotion_count, 2U);
        EXPECT_EQ(
            fixture.statistics->wavl_tree_insertion_single_rotation_count,
            0U);
        EXPECT_EQ(
            fixture.statistics->wavl_tree_insertion_double_rotation_count,
            1U);
        EXPECT_EQ(fixture.statistics->wavl_tree_insertion_fixup_step_count, 3U);
        EXPECT_EQ(fixture.statistics->wavl_tree_maximum_insertion_fixup_steps,
                  2U);
        EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
                  LzssWavlTreeValidationError::none) << text;
    }
}

TEST(LzssWavlTreeMatchFinder, RejectsInvalidInsertionWithoutMutation) {
    auto fixture = initialize_wavl("ABCDE___FGHIJ___");
    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::none);
    const auto original = std::vector<std::byte>(
        fixture.storage.bytes.begin(), fixture.storage.bytes.end());
    const auto original_insertion_count =
        fixture.statistics->wavl_tree_insertion_count;

    EXPECT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::invalid_state);
    EXPECT_EQ(insert_lzss_wavl_tree_position(
                  fixture.finder, fixture.input.size() - 4U),
              LzssWavlTreeError::invalid_position);
    EXPECT_TRUE(std::ranges::equal(fixture.storage.bytes, original));
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_count,
              original_insertion_count);
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);
}

TEST(LzssWavlTreeMatchFinder, ValidatorDetectsRankLinkAndMetadataDamage) {
    auto fixture = initialize_wavl("ABCxxxxx");
    for (const auto position : {0U, 1U, 2U}) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, position),
                  LzssWavlTreeError::none);
    }
    ASSERT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);

    auto ranks = mutable_array_at<std::uint8_t>(
        fixture.storage.bytes, fixture.required.rank_offset,
        fixture.required.node_count);
    auto parents = mutable_array_at<std::uint32_t>(
        fixture.storage.bytes, fixture.required.parent_offset,
        fixture.required.node_count);
    auto maximums = mutable_array_at<std::size_t>(
        fixture.storage.bytes,
        fixture.required.subtree_maximum_position_offset,
        fixture.required.node_count);
    const auto root = fixture.finder.root_index();

    ranks[root] = 3;
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::invalid_rank_difference);
    ranks[root] = 1;

    const auto left = inspect_lzss_wavl_tree_node(fixture.finder, root).left;
    ranks[left] = 1;
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::invalid_leaf_rank);
    ranks[left] = 0;

    parents[left] = lzss_wavl_tree_null_node;
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::cycle_or_disconnected);
    parents[left] = root;

    maximums[root] = 1;
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::invalid_subtree_maximum);
    maximums[root] = 2;
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);
}

TEST(LzssWavlTreeMatchFinder, RejectsBrokenReciprocalLinkWithoutMutation) {
    auto fixture = initialize_wavl("ABCDxxxxx");
    for (const auto position : {0U, 1U, 2U}) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, position),
                  LzssWavlTreeError::none);
    }
    const auto root = fixture.finder.root_index();
    const auto left = inspect_lzss_wavl_tree_node(fixture.finder, root).left;
    auto parents = mutable_array_at<std::uint32_t>(
        fixture.storage.bytes, fixture.required.parent_offset,
        fixture.required.node_count);
    parents[left] = left;
    const auto corrupted = std::vector<std::byte>(
        fixture.storage.bytes.begin(), fixture.storage.bytes.end());
    const auto insertion_count =
        fixture.statistics->wavl_tree_insertion_count;

    EXPECT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 3),
              LzssWavlTreeError::invalid_state);
    EXPECT_TRUE(std::ranges::equal(fixture.storage.bytes, corrupted));
    EXPECT_EQ(fixture.statistics->wavl_tree_insertion_count,
              insertion_count);
}

TEST(LzssWavlTreeMatchFinder, EveryInsertionStaysValidAndDeterministic) {
    std::vector<std::byte> input(256);
    std::uint32_t state = UINT32_C(0x9e3779b9);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto wavl_required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    const auto avl_required = calculate_lzss_binary_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(wavl_required.error, LzssWavlTreeError::none);
    ASSERT_EQ(avl_required.error, LzssBinaryTreeError::none);
    auto first_storage = make_storage(wavl_required.workspace_size);
    auto second_storage = make_storage(wavl_required.workspace_size);
    auto avl_storage = make_storage(avl_required.workspace_size);
    LzssWavlTreeMatchFinder first{};
    LzssWavlTreeMatchFinder second{};
    LzssBinaryTreeMatchFinder avl{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, first_storage.bytes, first),
              LzssWavlTreeError::none);
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssWavlTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, {}, {}, avl_storage.bytes, avl),
              LzssBinaryTreeError::none);

    const auto count = input.size() - lzss_wavl_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(first, position),
                  LzssWavlTreeError::none) << position;
        ASSERT_EQ(insert_lzss_wavl_tree_position(second, position),
                  LzssWavlTreeError::none) << position;
        ASSERT_EQ(insert_lzss_binary_tree_position(avl, position),
                  LzssBinaryTreeError::none) << position;
        ASSERT_EQ(validate_lzss_wavl_tree(first),
                  LzssWavlTreeValidationError::none) << position;
        ASSERT_EQ(validate_lzss_wavl_tree(second),
                  LzssWavlTreeValidationError::none) << position;
        ASSERT_EQ(validate_lzss_binary_tree(avl),
                  LzssBinaryTreeValidationError::none) << position;
        EXPECT_EQ(first.active_node_count(), avl.active_node_count());
        EXPECT_EQ(first.root_index(), second.root_index());
    }
    for (std::uint32_t node = 0; node < input.size(); ++node) {
        EXPECT_EQ(inspect_lzss_wavl_tree_node(first, node),
                  inspect_lzss_wavl_tree_node(second, node));
    }
    EXPECT_TRUE(std::ranges::equal(first_storage.bytes, second_storage.bytes));
}

TEST(LzssWavlTreeMatchFinder, LongOrderedInsertionPropagatesSafelyToRoot) {
    std::vector<std::byte> input(1'024, std::byte{'A'});
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssWavlTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, &statistics),
              LzssWavlTreeError::none);

    const auto count = input.size() - lzss_wavl_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(finder, position),
                  LzssWavlTreeError::none) << position;
        ASSERT_EQ(validate_lzss_wavl_tree(finder),
                  LzssWavlTreeValidationError::none) << position;
    }

    ASSERT_NE(finder.root_index(), lzss_wavl_tree_null_node);
    const auto root = inspect_lzss_wavl_tree_node(
        finder, finder.root_index());
    EXPECT_LE(root.rank, 2U * std::bit_width(count));
    EXPECT_EQ(root.subtree_maximum_position, count - 1U);
    EXPECT_EQ(statistics.wavl_tree_insertion_count, count);
    EXPECT_GT(statistics.wavl_tree_maximum_insertion_fixup_steps, 2U);
}

TEST(LzssWavlTreeMatchFinder, RemovalHandlesD0AndRankOneLeafD1) {
    auto fixture = initialize_wavl("ABxxxxxx");
    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::none);
    ASSERT_EQ(remove_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::none);
    EXPECT_TRUE(fixture.finder.empty());
    EXPECT_EQ(fixture.finder.root_index(), lzss_wavl_tree_null_node);
    EXPECT_EQ(fixture.statistics->wavl_tree_removal_fixup_step_count, 0U);

    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 0),
              LzssWavlTreeError::none);
    ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, 1),
              LzssWavlTreeError::none);
    ASSERT_EQ(remove_lzss_wavl_tree_position(fixture.finder, 1),
              LzssWavlTreeError::none);
    const auto root = inspect_lzss_wavl_tree_node(
        fixture.finder, fixture.finder.root_index());
    EXPECT_EQ(root.rank, 0U);
    EXPECT_EQ(fixture.statistics->wavl_tree_removal_demotion_count, 1U);
    EXPECT_EQ(fixture.statistics->wavl_tree_removal_fixup_step_count, 1U);
    EXPECT_EQ(fixture.statistics->wavl_tree_retirement_count, 2U);
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);
}

TEST(LzssWavlTreeMatchFinder, RemovalUsesPhysicalSuccessorSlots) {
    for (const auto text : {"BCAxxxxxx", "DBFEGxxxxxx"}) {
        auto fixture = initialize_wavl(text);
        const auto count = std::string_view{text}.size()
            - lzss_wavl_tree_prefix_size + 1U;
        for (std::size_t position = 0; position < count; ++position) {
            ASSERT_EQ(insert_lzss_wavl_tree_position(
                          fixture.finder, position),
                      LzssWavlTreeError::none) << text << position;
        }
        const auto removed = fixture.finder.root_index();
        const auto removed_position = inspect_lzss_wavl_tree_node(
            fixture.finder, removed).position;
        ASSERT_NE(inspect_lzss_wavl_tree_node(
                      fixture.finder, removed).left,
                  lzss_wavl_tree_null_node) << text;
        ASSERT_NE(inspect_lzss_wavl_tree_node(
                      fixture.finder, removed).right,
                  lzss_wavl_tree_null_node) << text;

        ASSERT_EQ(remove_lzss_wavl_tree_position(
                      fixture.finder, removed_position),
                  LzssWavlTreeError::none) << text;
        EXPECT_EQ(inspect_lzss_wavl_tree_node(fixture.finder, removed),
                  LzssWavlTreeNodeSnapshot{}) << text;
        EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
                  LzssWavlTreeValidationError::none) << text;
    }
}

TEST(LzssWavlTreeMatchFinder, EveryRemovalStaysValidAndBounded) {
    std::vector<std::byte> input(256);
    std::uint32_t state = UINT32_C(0x243f6a88);
    for (auto& value : input) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    const auto required = calculate_lzss_wavl_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssWavlTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssMatchFinderStatistics statistics{};
    LzssWavlTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, &statistics),
              LzssWavlTreeError::none);

    const auto count = input.size() - lzss_wavl_tree_prefix_size + 1U;
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(finder, position),
                  LzssWavlTreeError::none) << position;
    }
    for (std::size_t position = 0; position < count; ++position) {
        ASSERT_EQ(remove_lzss_wavl_tree_position(finder, position),
                  LzssWavlTreeError::none) << position;
        ASSERT_EQ(validate_lzss_wavl_tree(finder),
                  LzssWavlTreeValidationError::none) << position;
        EXPECT_EQ(finder.active_node_count(), count - position - 1U);
    }
    EXPECT_TRUE(finder.empty());
    EXPECT_EQ(statistics.wavl_tree_retirement_count, count);
    EXPECT_LE(statistics.wavl_tree_maximum_removal_fixup_steps,
              2U * std::bit_width(count));
    const auto preflight_bound = 8U * std::bit_width(count) + 4U;
    EXPECT_LE(statistics.wavl_tree_maximum_removal_preflight_nodes,
              preflight_bound);
    EXPECT_LE(statistics.wavl_tree_removal_preflight_node_count,
              count * preflight_bound);
    EXPECT_GT(statistics.wavl_tree_removal_demotion_count, 0U);
    EXPECT_GT(statistics.wavl_tree_removal_single_rotation_count, 0U);
    EXPECT_GT(statistics.wavl_tree_removal_double_rotation_count, 0U);
}

TEST(LzssWavlTreeMatchFinder, RejectsInvalidRemovalWithoutMutation) {
    auto fixture = initialize_wavl("ABCxxxxx");
    for (const auto position : {0U, 1U, 2U}) {
        ASSERT_EQ(insert_lzss_wavl_tree_position(fixture.finder, position),
                  LzssWavlTreeError::none);
    }
    auto parents = mutable_array_at<std::uint32_t>(
        fixture.storage.bytes, fixture.required.parent_offset,
        fixture.required.node_count);
    const auto root = fixture.finder.root_index();
    const auto left = inspect_lzss_wavl_tree_node(fixture.finder, root).left;
    parents[left] = left;
    const auto corrupted = std::vector<std::byte>(
        fixture.storage.bytes.begin(), fixture.storage.bytes.end());

    EXPECT_EQ(remove_lzss_wavl_tree_position(fixture.finder, 2),
              LzssWavlTreeError::invalid_state);
    EXPECT_TRUE(std::ranges::equal(fixture.storage.bytes, corrupted));
    EXPECT_EQ(
        fixture.statistics->wavl_tree_removal_preflight_node_count, 0U);
    EXPECT_EQ(fixture.statistics->wavl_tree_retirement_count, 0U);
}

TEST(LzssWavlTreeMatchFinder, ExactQueriesEqualAvlAcrossInputClasses) {
    LzssParameters parameters{};
    parameters.window_size = 17;
    parameters.max_match_length = 31;
    expect_wavl_queries_equal_avl(bytes(""), parameters);
    expect_wavl_queries_equal_avl(bytes("A"), parameters);
    expect_wavl_queries_equal_avl(
        bytes("ABABABABABABABABABABABABABABABAB"), parameters);
    expect_wavl_queries_equal_avl(
        bytes("ABCDE1ABCDE2ABCDE3ABCDE4ABCDE5ABCDE6"), parameters);
    expect_wavl_queries_equal_avl(
        bytes("AAAAABAAAACAAAAADAAAAEAAAAAFAAAAAGAAAAAH"), parameters);

    std::vector<std::byte> pseudorandom(512);
    std::uint32_t state = UINT32_C(0x6a09e667);
    for (auto& value : pseudorandom) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24U);
    }
    expect_wavl_queries_equal_avl(pseudorandom, parameters);
}

TEST(LzssWavlTreeMatchFinder, BulkAdvancementMatchesAvl) {
    const auto input = bytes(
        "ABABABABABABABABXYZABABABABABCDE1ABCDE2ABCDE3");
    LzssParameters parameters{};
    parameters.window_size = 16;
    const auto wavl_required = calculate_lzss_wavl_tree_workspace(
        input.size(), parameters, {});
    const auto avl_required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(wavl_required.error, LzssWavlTreeError::none);
    ASSERT_EQ(avl_required.error, LzssBinaryTreeError::none);
    auto wavl_storage = make_storage(wavl_required.workspace_size);
    auto avl_storage = make_storage(avl_required.workspace_size);
    LzssWavlTreeMatchFinder wavl{};
    LzssBinaryTreeMatchFinder avl{};
    ASSERT_EQ(initialize_lzss_wavl_tree_match_finder(
                  input, parameters, {}, wavl_storage.bytes, wavl),
              LzssWavlTreeError::none);
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, avl_storage.bytes, avl),
              LzssBinaryTreeError::none);

    const std::array<std::size_t, 6> landings{
        0U, 2U, 16U, 19U, 41U, input.size()};
    for (std::size_t index = 0; index < landings.size(); ++index) {
        const auto position = landings[index];
        EXPECT_EQ(wavl.find_match(position), avl.find_match(position));
        if (index + 1U == landings.size()) continue;
        const auto next = landings[index + 1U];
        wavl.advance(position, next);
        avl.advance(position, next);
        ASSERT_EQ(validate_lzss_wavl_tree(wavl),
                  LzssWavlTreeValidationError::none);
        ASSERT_EQ(validate_lzss_binary_tree(avl),
                  LzssBinaryTreeValidationError::none);
    }
}

TEST(LzssWavlTreeMatchFinder, InvalidProtocolStateIsSticky) {
    auto fixture = initialize_wavl("ABCDE1ABCDE2ABCDE3");
    EXPECT_EQ(fixture.finder.find_neighbors(1).error,
              LzssWavlTreeError::invalid_state);
    fixture.finder.advance(1, 2);
    EXPECT_FALSE(fixture.finder.state_valid());
    EXPECT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::invalid_protocol_state);
    EXPECT_EQ(fixture.finder.find_candidate(fixture.input.size()).error,
              LzssWavlTreeError::invalid_state);
    fixture.finder.advance(fixture.input.size(), fixture.input.size());
    EXPECT_FALSE(fixture.finder.state_valid());
}

TEST(LzssWavlTreeMatchFinder, BoundedQueryRejectsCyclicTopology) {
    auto fixture = initialize_wavl("ABCDE1ABCDE2ABCDE3ABCDE4");
    fixture.finder.advance(0, 16);
    ASSERT_EQ(validate_lzss_wavl_tree(fixture.finder),
              LzssWavlTreeValidationError::none);
    const auto root = fixture.finder.root_index();
    auto left = mutable_array_at<std::uint32_t>(
        fixture.storage.bytes, fixture.required.left_offset,
        fixture.required.node_count);
    auto right = mutable_array_at<std::uint32_t>(
        fixture.storage.bytes, fixture.required.right_offset,
        fixture.required.node_count);
    left[root] = root;
    right[root] = root;

    EXPECT_EQ(fixture.finder.find_candidate(16).error,
              LzssWavlTreeError::invalid_state);
}

TEST(LzssWavlTreeMatchFinder, ReportsBoundedQueryStatistics) {
    auto fixture = initialize_wavl(
        "ABCDE1ABCDE2ABCDE3ABCDE4ABCDE5ABCDE6ABCDE7");
    for (std::size_t position = 0; position <= fixture.input.size();
         ++position) {
        static_cast<void>(fixture.finder.find_match(position));
        if (position != fixture.input.size()) {
            fixture.finder.advance(position, position + 1U);
            ASSERT_TRUE(fixture.finder.state_valid()) << position;
        }
    }
    EXPECT_EQ(fixture.statistics->query_count,
              fixture.input.size() + 1U);
    EXPECT_GT(fixture.statistics->wavl_tree_key_comparison_count, 0U);
    EXPECT_GT(fixture.statistics->wavl_tree_key_byte_comparison_count, 0U);
    EXPECT_GT(fixture.statistics->wavl_tree_lcp_byte_comparison_count, 0U);
    EXPECT_GT(
        fixture.statistics->wavl_tree_prefix_range_comparison_count, 0U);
    EXPECT_GT(fixture.statistics->wavl_tree_maximum_nodes_per_query, 0U);
    std::uint64_t histogram_queries{};
    for (const auto count :
         fixture.statistics->wavl_tree_query_depth_histogram) {
        histogram_queries += count;
    }
    EXPECT_EQ(histogram_queries, fixture.statistics->query_count);
    EXPECT_FALSE(fixture.statistics->overflowed);
}

} // namespace

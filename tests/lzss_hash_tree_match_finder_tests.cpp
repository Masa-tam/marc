#include "dictionary/lzss_hash_tree_match_finder.hpp"
#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
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

void expect_all_bytes(
    const std::span<const std::byte> values,
    const std::byte expected) {
    EXPECT_TRUE(std::ranges::all_of(values, [expected](const auto value) {
        return value == expected;
    }));
}

void expect_hash_tree_matches_existing_finders(
    const std::span<const std::byte> input,
    const LzssParameters& parameters = {}) {
    const auto tree_required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(tree_required.error, LzssHashTreeError::none);
    auto tree_storage = make_storage(tree_required.workspace_size);
    LzssHashTreeMatchFinder tree{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, parameters, {},
                  tree_storage.bytes.first(tree_required.workspace_size),
                  tree),
              LzssHashTreeError::none);

    const auto chain_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(chain_required.error, LzssHashChainError::none);
    auto chain_storage = make_storage(chain_required.workspace_size);
    LzssHashChainMatchFinder chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {},
                  chain_storage.bytes.first(chain_required.workspace_size),
                  chain),
              LzssHashChainError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    for (std::size_t position = 0; position <= input.size(); ++position) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(tree.find_match(position), expected) << position;
        EXPECT_EQ(chain.find_match(position), expected) << position;
        EXPECT_TRUE(tree.state_valid()) << position;
        EXPECT_EQ(tree.last_error(), LzssHashTreeError::none) << position;
        if (position != input.size()) {
            tree.advance(position, position + 1U);
            chain.advance(position, position + 1U);
            exhaustive.advance(position, position + 1U);
        }
    }
}

[[nodiscard]] constexpr std::size_t align_size(
    const std::size_t value, const std::size_t alignment) noexcept {
    const auto remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

TEST(LzssHashTreeMatchFinder, CalculatesEmptyAndFiveByteWorkspace) {
    auto required = calculate_lzss_hash_tree_workspace(4, {}, {});
    EXPECT_EQ(required.error, LzssHashTreeError::none);
    EXPECT_EQ(required.workspace_size, 0U);
    EXPECT_EQ(required.bucket_count, 0U);
    EXPECT_EQ(required.node_count, 0U);

    required = calculate_lzss_hash_tree_workspace(5, {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    EXPECT_EQ(required.bucket_count, 8U);
    EXPECT_EQ(required.node_count, 5U);
    EXPECT_EQ(required.head_offset, 0U);
    EXPECT_EQ(required.link_offset,
              8U * sizeof(LzssHashTreeStoredPosition));
    EXPECT_EQ(required.root_offset,
              required.link_offset + 5U * sizeof(std::uint32_t));
    EXPECT_EQ(required.mode_offset,
              required.root_offset + 8U * sizeof(std::uint32_t));
    EXPECT_EQ(required.left_offset,
              align_size(required.mode_offset + 8U, alignof(std::uint32_t)));
    EXPECT_EQ(required.right_offset,
              required.left_offset + 5U * sizeof(std::uint32_t));
    EXPECT_EQ(required.parent_offset,
              required.right_offset + 5U * sizeof(std::uint32_t));
    EXPECT_EQ(required.height_offset,
              required.parent_offset + 5U * sizeof(std::uint32_t));
    EXPECT_EQ(required.position_offset,
              align_size(required.height_offset + 5U,
                         alignof(LzssHashTreeStoredPosition)));
    EXPECT_EQ(required.subtree_maximum_position_offset,
              align_size(required.position_offset
                             + 5U * sizeof(LzssHashTreeStoredPosition),
                         alignof(std::size_t)));
    EXPECT_EQ(required.workspace_size,
              required.subtree_maximum_position_offset
                  + 5U * sizeof(std::size_t));
}

TEST(LzssHashTreeMatchFinder, FixesLargeWorkspaceLayoutAndBound) {
    auto required = calculate_lzss_hash_tree_workspace(65'536, {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    EXPECT_EQ(required.bucket_count, 65'536U);
    EXPECT_EQ(required.node_count, 65'536U);
    EXPECT_EQ(required.workspace_alignment,
              std::max(alignof(std::size_t), alignof(std::uint32_t)));
    EXPECT_EQ(required.workspace_size,
              (22U + sizeof(std::size_t)
                   + 2U * sizeof(LzssHashTreeStoredPosition))
                  * static_cast<std::size_t>(65'536));
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(required.workspace_size, 2'490'368U);
    }

    LzssParameters parameters{};
    parameters.window_size = 1U << 20;
    required = calculate_lzss_hash_tree_workspace(
        1U << 20, parameters, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    EXPECT_EQ(required.bucket_count, 65'536U);
    EXPECT_EQ(required.node_count, 1U << 20);
    const auto expected = static_cast<std::size_t>(1U << 20)
            * (17U + sizeof(std::size_t)
                + sizeof(LzssHashTreeStoredPosition))
        + static_cast<std::size_t>(65'536)
            * (5U + sizeof(LzssHashTreeStoredPosition));
    EXPECT_EQ(required.workspace_size, expected);
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(required.workspace_size, 30'998'528U);
    }
}

TEST(LzssHashTreeMatchFinder, AlignsAndSeparatesEveryArray) {
    const auto required = calculate_lzss_hash_tree_workspace(37, {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);

    struct Segment {
        std::size_t offset;
        std::size_t count;
        std::size_t element_size;
        std::size_t alignment;
    };
    const std::array segments{
        Segment{required.head_offset, required.bucket_count,
                sizeof(LzssHashTreeStoredPosition),
                alignof(LzssHashTreeStoredPosition)},
        Segment{required.link_offset, required.node_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.root_offset, required.bucket_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.mode_offset, required.bucket_count,
                sizeof(std::uint8_t), alignof(std::uint8_t)},
        Segment{required.left_offset, required.node_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.right_offset, required.node_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.parent_offset, required.node_count,
                sizeof(std::uint32_t), alignof(std::uint32_t)},
        Segment{required.height_offset, required.node_count,
                sizeof(std::uint8_t), alignof(std::uint8_t)},
        Segment{required.position_offset, required.node_count,
                sizeof(LzssHashTreeStoredPosition),
                alignof(LzssHashTreeStoredPosition)},
        Segment{required.subtree_maximum_position_offset,
                required.node_count, sizeof(std::size_t),
                alignof(std::size_t)},
    };

    std::size_t previous_end{};
    for (const auto& segment : segments) {
        EXPECT_EQ(segment.offset % segment.alignment, 0U);
        EXPECT_GE(segment.offset, previous_end);
        previous_end = segment.offset + segment.count * segment.element_size;
    }
    EXPECT_EQ(previous_end, required.workspace_size);
}

TEST(LzssHashTreeMatchFinder, RejectsInvalidAndUnboundedRequirements) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 0;
    EXPECT_EQ(calculate_lzss_hash_tree_workspace(16, {}, limits).error,
              LzssHashTreeError::invalid_limits);

    LzssParameters parameters{};
    parameters.min_match_length = 4;
    const auto invalid_parameters = calculate_lzss_hash_tree_workspace(
        16, parameters, {});
    EXPECT_EQ(invalid_parameters.error,
              LzssHashTreeError::invalid_parameters);
    EXPECT_EQ(invalid_parameters.format_error,
              LzssFormatError::invalid_match_range);

    limits = {};
    limits.max_frame_size = 64;
    EXPECT_EQ(calculate_lzss_hash_tree_workspace(65, {}, limits).error,
              LzssHashTreeError::input_limit_exceeded);

    limits = {};
    limits.max_internal_buffered_bytes = 1U << 20;
    EXPECT_EQ(calculate_lzss_hash_tree_workspace(
                  65'536, {}, limits).error,
              LzssHashTreeError::workspace_limit_exceeded);

    limits = {};
    limits.max_total_output_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_frame_size = std::numeric_limits<std::uint64_t>::max();
    limits.max_internal_buffered_bytes =
        std::numeric_limits<std::uint64_t>::max();
    if constexpr (std::numeric_limits<std::size_t>::max()
                  > std::numeric_limits<std::uint32_t>::max()) {
        const auto maximum_stored_extent = static_cast<std::size_t>(
            std::numeric_limits<LzssHashTreeStoredPosition>::max());
        EXPECT_EQ(calculate_lzss_hash_tree_workspace(
                      maximum_stored_extent, {}, limits).error,
                  LzssHashTreeError::none);
        EXPECT_EQ(calculate_lzss_hash_tree_workspace(
                      maximum_stored_extent + 1U, {}, limits).error,
                  LzssHashTreeError::arithmetic_overflow);
    }
    EXPECT_EQ(calculate_lzss_hash_tree_workspace(
                  std::numeric_limits<std::size_t>::max(), {}, limits).error,
              LzssHashTreeError::arithmetic_overflow);
}

TEST(LzssHashTreeMatchFinder, InitializesOnlyControlArrays) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3ABCDE4");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    auto storage = make_storage(required.workspace_size + 16U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});

    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssHashTreeError::none);

    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.bucket_count(), required.bucket_count);
    EXPECT_EQ(finder.node_capacity(), required.node_count);
    EXPECT_EQ(finder.next_position(), 0U);

    const auto active = std::span<const std::byte>{storage.bytes}
        .first(required.workspace_size);
    const auto heads = array_at<LzssHashTreeStoredPosition>(
        active, required.head_offset, required.bucket_count);
    EXPECT_TRUE(std::ranges::all_of(heads, [](const auto value) {
        return value == lzss_hash_tree_no_stored_position;
    }));
    const auto roots = array_at<std::uint32_t>(
        active, required.root_offset, required.bucket_count);
    EXPECT_TRUE(std::ranges::all_of(roots, [](const auto value) {
        return value == lzss_hash_tree_null_node;
    }));
    const auto modes = array_at<LzssHashTreeBucketMode>(
        active, required.mode_offset, required.bucket_count);
    EXPECT_TRUE(std::ranges::all_of(modes, [](const auto value) {
        return value == LzssHashTreeBucketMode::chain;
    }));

    expect_all_bytes(
        active.subspan(required.link_offset,
                       required.root_offset - required.link_offset),
        std::byte{0xa5});
    const auto mode_end = required.mode_offset + required.bucket_count;
    expect_all_bytes(
        active.subspan(mode_end, required.left_offset - mode_end),
        std::byte{0xa5});
    expect_all_bytes(active.subspan(required.left_offset), std::byte{0xa5});
    expect_all_bytes(storage.bytes.subspan(required.workspace_size),
                     std::byte{0xa5});
}

TEST(LzssHashTreeMatchFinder, InitializesShortInputWithoutWorkspace) {
    const auto input = bytes("ABCD");
    LzssHashTreeMatchFinder finder{};

    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, {}, finder),
              LzssHashTreeError::none);
    EXPECT_TRUE(finder.initialized());
    EXPECT_TRUE(finder.state_valid());
    EXPECT_EQ(finder.input_size(), input.size());
    EXPECT_EQ(finder.bucket_count(), 0U);
    EXPECT_EQ(finder.node_capacity(), 0U);
    EXPECT_EQ(finder.next_position(), 0U);
}

TEST(LzssHashTreeMatchFinder, RejectsWorkspaceFailuresAtomically) {
    const auto seed_input = bytes("ABCD");
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  seed_input, {}, {}, {}, finder),
              LzssHashTreeError::none);

    const auto input = bytes("ABCDE1ABCDE2ABCDE3ABCDE4");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    auto storage = make_storage(required.workspace_size + 1U);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    const auto original_storage = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    auto invalid_parameters = LzssParameters{};
    invalid_parameters.min_match_length = 4;
    EXPECT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, invalid_parameters, {}, storage.bytes, finder),
              LzssHashTreeError::invalid_parameters);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_EQ(finder.bucket_count(), 0U);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {},
                  storage.bytes.first(required.workspace_size - 1U), finder),
              LzssHashTreeError::workspace_too_small);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_EQ(finder.bucket_count(), 0U);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    EXPECT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {},
                  storage.bytes.subspan(1U, required.workspace_size), finder),
              LzssHashTreeError::misaligned_workspace);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original_storage));

    auto arena = make_storage(required.workspace_size + input.size());
    std::ranges::fill(arena.bytes, std::byte{0xa5});
    std::ranges::copy(input, arena.bytes.begin());
    const auto aliased_input = std::span<const std::byte>{arena.bytes}
        .first(input.size());
    const auto original_arena = std::vector<std::byte>(
        arena.bytes.begin(), arena.bytes.end());
    EXPECT_EQ(initialize_lzss_hash_tree_match_finder(
                  aliased_input, {}, {},
                  arena.bytes.first(required.workspace_size), finder),
              LzssHashTreeError::overlapping_buffers);
    EXPECT_EQ(finder.input_size(), seed_input.size());
    EXPECT_TRUE(std::ranges::equal(arena.bytes, original_arena));
}

TEST(LzssHashTreeMatchFinder, ChainPathMatchesExistingExactFinders) {
    const auto empty = bytes("");
    expect_hash_tree_matches_existing_finders(empty);
    const auto short_input = bytes("ABCD");
    expect_hash_tree_matches_existing_finders(short_input);
    const auto repetitive = bytes("ABABABABABABABABABABABABABAB");
    expect_hash_tree_matches_existing_finders(repetitive);
    const auto collision = std::array{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}, std::byte{0}, std::byte{0x20}, std::byte{0},
        std::byte{0x58}, std::byte{0x59}, std::byte{1}, std::byte{0},
        std::byte{0}, std::byte{0x58}, std::byte{0x59}};
    expect_hash_tree_matches_existing_finders(collision);

    std::vector<std::byte> binary(512);
    for (std::size_t index = 0; index < binary.size(); ++index) {
        binary[index] = static_cast<std::byte>(index & 0xffU);
    }
    for (const auto window : {5U, 17U, 64U}) {
        LzssParameters parameters{};
        parameters.window_size = window;
        expect_hash_tree_matches_existing_finders(binary, parameters);
    }
}

TEST(LzssHashTreeMatchFinder, ChainPathIndexesSkippedPositions) {
    const auto input = bytes("ABABABABABABABABXYZABABABAB");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssHashTreeError::none);
    LzssExhaustiveMatchFinder exhaustive{input, {}};

    EXPECT_EQ(finder.find_match(0), exhaustive.find_match(0));
    finder.advance(0, 2);
    exhaustive.advance(0, 2);
    EXPECT_EQ(finder.find_match(2), exhaustive.find_match(2));
    finder.advance(2, 16);
    exhaustive.advance(2, 16);
    EXPECT_EQ(finder.find_match(16), exhaustive.find_match(16));
    EXPECT_TRUE(finder.state_valid());
}

TEST(LzssHashTreeMatchFinder, ChainPathMatchesHashChainStatistics) {
    const auto input = bytes("ABCDEABCDEABCDE");
    const auto tree_required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto tree_storage = make_storage(tree_required.workspace_size);
    LzssMatchFinderStatistics tree_statistics{};
    LzssHashTreeMatchFinder tree{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, tree_storage.bytes, tree, &tree_statistics),
              LzssHashTreeError::none);

    const auto chain_required = calculate_lzss_hash_chain_workspace(
        input.size(), {}, {});
    auto chain_storage = make_storage(chain_required.workspace_size);
    LzssMatchFinderStatistics chain_statistics{};
    LzssHashChainMatchFinder chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, {}, {}, chain_storage.bytes, chain,
                  &chain_statistics),
              LzssHashChainError::none);

    for (std::size_t position = 0; position < input.size(); ++position) {
        EXPECT_EQ(tree.find_match(position), chain.find_match(position));
        tree.advance(position, position + 1U);
        chain.advance(position, position + 1U);
    }
    EXPECT_EQ(tree_statistics.query_count, chain_statistics.query_count);
    EXPECT_EQ(tree_statistics.candidate_count,
              chain_statistics.candidate_count);
    EXPECT_EQ(tree_statistics.byte_comparison_count,
              chain_statistics.byte_comparison_count);
    EXPECT_EQ(tree_statistics.hash_chain_prefix_match_count,
              chain_statistics.hash_chain_prefix_match_count);
    EXPECT_EQ(tree_statistics.hash_chain_prefix_mismatch_count,
              chain_statistics.hash_chain_prefix_mismatch_count);
    EXPECT_EQ(tree_statistics.hash_chain_extension_byte_comparison_count,
              chain_statistics.hash_chain_extension_byte_comparison_count);
    EXPECT_EQ(tree_statistics.hash_chain_maximum_candidates_per_query,
              chain_statistics.hash_chain_maximum_candidates_per_query);
    EXPECT_EQ(tree_statistics.hash_chain_query_depth_histogram,
              chain_statistics.hash_chain_query_depth_histogram);
    EXPECT_EQ(tree_statistics.hash_tree_chain_query_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_chain_candidate_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_trigger_query_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_tree_query_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_promotion_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_promotion_build_node_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_tree_query_node_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_insertion_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_retirement_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_promotion_build_key_comparison_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_promotion_build_key_byte_comparison_count,
        0U);
    EXPECT_EQ(tree_statistics.hash_tree_promotion_build_rotation_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_tree_query_key_comparison_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_tree_query_key_byte_comparison_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_tree_query_lcp_byte_comparison_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_tree_query_prefix_range_comparison_count,
        0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_tree_query_prefix_range_byte_comparison_count,
        0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_tree_query_lcp_skipped_byte_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_maintenance_key_comparison_count, 0U);
    EXPECT_EQ(
        tree_statistics.hash_tree_maintenance_key_byte_comparison_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_rotation_count, 0U);
    EXPECT_EQ(tree_statistics.hash_tree_maximum_height, 0U);
    EXPECT_TRUE(std::ranges::all_of(
        tree_statistics.hash_tree_chain_query_depth_histogram,
        [](const auto value) { return value == 0; }));
    EXPECT_TRUE(std::ranges::all_of(
        tree_statistics.hash_tree_tree_query_depth_histogram,
        [](const auto value) { return value == 0; }));
}

TEST(LzssHashTreeMatchFinder, ConstructsLazyLinkBeforePublishingHead) {
    const auto input = bytes("ABCDEABCDE");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    std::ranges::fill(storage.bytes, std::byte{0xa5});
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssHashTreeError::none);

    finder.advance(0, 1);
    ASSERT_TRUE(finder.state_valid());
    const auto links = array_at<std::uint32_t>(
        storage.bytes, required.link_offset, 1);
    EXPECT_EQ(links[0], 0U);
    expect_all_bytes(
        storage.bytes.subspan(required.link_offset + sizeof(std::uint32_t),
                              sizeof(std::uint32_t)),
        std::byte{0xa5});
    const auto hash = calculate_lzss_prefix_hash(input, 0);
    ASSERT_TRUE(hash.valid);
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (required.bucket_count - 1U);
    const auto heads = array_at<LzssHashTreeStoredPosition>(
        storage.bytes, required.head_offset, required.bucket_count);
    EXPECT_EQ(heads[bucket], 0U);
}

TEST(LzssHashTreeMatchFinder, ProtocolFailuresAreStickyAndFinite) {
    const auto input = bytes("ABCDEABCDE");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssHashTreeError::none);
    const auto original = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    EXPECT_EQ(finder.find_match(1), LzssMatch{});
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(), LzssHashTreeError::invalid_protocol);
    EXPECT_EQ(finder.find_match(0), LzssMatch{});
    finder.advance(0, 1);
    EXPECT_EQ(finder.last_error(), LzssHashTreeError::invalid_protocol);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, original));

    LzssHashTreeMatchFinder uninitialized{};
    EXPECT_EQ(uninitialized.find_match(0), LzssMatch{});
    EXPECT_EQ(uninitialized.last_error(), LzssHashTreeError::invalid_state);

    auto second_storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder second{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, second_storage.bytes, second),
              LzssHashTreeError::none);
    second.advance(1, 2);
    EXPECT_FALSE(second.state_valid());
    EXPECT_EQ(second.last_error(), LzssHashTreeError::invalid_protocol);
}

TEST(LzssHashTreeMatchFinder, ReachableCorruptionBecomesStickyInvalid) {
    const auto input = std::array{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0x58},
        std::byte{0x59}, std::byte{0}, std::byte{0x20}, std::byte{0},
        std::byte{0x58}, std::byte{0x59}, std::byte{1}, std::byte{0},
        std::byte{0}, std::byte{0x58}, std::byte{0x59}};
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder),
              LzssHashTreeError::none);
    finder.advance(0, 10);
    ASSERT_TRUE(finder.state_valid());

    const auto hash = calculate_lzss_prefix_hash(input, 10);
    ASSERT_TRUE(hash.valid);
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (required.bucket_count - 1U);
    auto heads = mutable_array_at<LzssHashTreeStoredPosition>(
        storage.bytes, required.head_offset, required.bucket_count);
    const auto reachable_head = heads[bucket];
    ASSERT_LT(reachable_head, 10U);
    ASSERT_NE(input[reachable_head], input[10]);
    auto link = mutable_array_at<std::uint32_t>(
        storage.bytes,
        required.link_offset
            + reachable_head * sizeof(std::uint32_t), 1);
    link[0] = static_cast<std::uint32_t>(reachable_head + 1U);
    const auto corrupted = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());

    EXPECT_EQ(finder.find_match(10), LzssMatch{});
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(), LzssHashTreeError::invalid_state);
    finder.advance(10, 11);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, corrupted));

    auto head_storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder head_finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, head_storage.bytes, head_finder),
              LzssHashTreeError::none);
    head_finder.advance(0, 10);
    auto bad_heads = mutable_array_at<LzssHashTreeStoredPosition>(
        head_storage.bytes, required.head_offset, required.bucket_count);
    bad_heads[bucket] = static_cast<LzssHashTreeStoredPosition>(input.size());
    EXPECT_EQ(head_finder.find_match(10), LzssMatch{});
    EXPECT_FALSE(head_finder.state_valid());
    EXPECT_EQ(head_finder.last_error(), LzssHashTreeError::invalid_state);
}

void expect_integrated_hash_tree_matches_exact_finders(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const bool skip_matches) {
    const auto tree_required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, {});
    ASSERT_EQ(tree_required.error, LzssHashTreeError::none);
    auto tree_storage = make_storage(tree_required.workspace_size);
    LzssHashTreeMatchFinder tree{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, parameters, {}, tree_storage.bytes, tree, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);

    const auto chain_required = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, {});
    auto chain_storage = make_storage(chain_required.workspace_size);
    LzssHashChainMatchFinder chain{};
    ASSERT_EQ(initialize_lzss_hash_chain_match_finder(
                  input, parameters, {}, chain_storage.bytes, chain),
              LzssHashChainError::none);

    const auto binary_required = calculate_lzss_binary_tree_workspace(
        input.size(), parameters, {});
    auto binary_storage = make_storage(binary_required.workspace_size);
    LzssBinaryTreeMatchFinder binary{};
    ASSERT_EQ(initialize_lzss_binary_tree_match_finder(
                  input, parameters, {}, binary_storage.bytes, binary),
              LzssBinaryTreeError::none);
    LzssExhaustiveMatchFinder exhaustive{input, parameters};

    std::size_t position{};
    while (true) {
        const auto expected = exhaustive.find_match(position);
        EXPECT_EQ(tree.find_match(position), expected) << position;
        EXPECT_EQ(chain.find_match(position), expected) << position;
        EXPECT_EQ(binary.find_match(position), expected) << position;
        ASSERT_TRUE(tree.state_valid()) << position;
        if (position == input.size()) break;
        auto step = std::size_t{1};
        if (skip_matches && expected.length != 0) step = expected.length;
        const auto next = std::min(input.size(), position + step);
        tree.advance(position, next);
        chain.advance(position, next);
        binary.advance(position, next);
        exhaustive.advance(position, next);
        ASSERT_TRUE(tree.state_valid()) << position;
        position = next;
    }
}

TEST(LzssHashTreeMatchFinder, IntegratedRouteMatchesAllExactFinders) {
    LzssParameters parameters{};
    parameters.window_size = 16;
    parameters.max_match_length = 12;
    const auto repetitive = bytes(
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    expect_integrated_hash_tree_matches_exact_finders(
        repetitive, parameters, false);
    expect_integrated_hash_tree_matches_exact_finders(
        repetitive, parameters, true);

    const auto periodic = bytes(
        "ABCDEABCDEABCDEABCDEABCDEABCDEABCDEABCDE");
    expect_integrated_hash_tree_matches_exact_finders(
        periodic, parameters, false);
    expect_integrated_hash_tree_matches_exact_finders(
        periodic, parameters, true);

    std::vector<std::byte> binary(96);
    for (std::size_t index = 0; index < binary.size(); ++index) {
        binary[index] = static_cast<std::byte>(
            (index * 73U + index / 7U) & 0xffU);
    }
    expect_integrated_hash_tree_matches_exact_finders(
        binary, parameters, false);
}

TEST(LzssHashTreeMatchFinder, TriggerPublishesOnlyDuringFollowingAdvance) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    auto roots = mutable_array_at<std::uint32_t>(
        storage.bytes, required.root_offset, required.bucket_count);
    auto modes = mutable_array_at<LzssHashTreeBucketMode>(
        storage.bytes, required.mode_offset, required.bucket_count);

    EXPECT_EQ(finder.find_match(0), LzssMatch{});
    finder.advance(0, 1);
    LzssExhaustiveMatchFinder trigger_oracle{input, {}};
    trigger_oracle.advance(0, 1);
    const auto expected = trigger_oracle.find_match(1);
    EXPECT_EQ(finder.find_match(1), expected);
    const auto hash = calculate_lzss_prefix_hash(input, 1);
    ASSERT_TRUE(hash.valid);
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (required.bucket_count - 1U);
    EXPECT_EQ(modes[bucket], LzssHashTreeBucketMode::chain);
    EXPECT_EQ(roots[bucket], lzss_hash_tree_null_node);

    finder.advance(1, 2);
    ASSERT_TRUE(finder.state_valid());
    EXPECT_EQ(modes[bucket], LzssHashTreeBucketMode::promoted_tree);
    EXPECT_NE(roots[bucket], lzss_hash_tree_null_node);

    LzssExhaustiveMatchFinder exhaustive{input, {}};
    exhaustive.advance(0, 2);
    EXPECT_EQ(finder.find_match(2), exhaustive.find_match(2));
}

TEST(LzssHashTreeMatchFinder, StatisticsDoNotChangePromotionState) {
    const auto input = bytes(
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto plain_storage = make_storage(required.workspace_size);
    auto counted_storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder plain{};
    LzssHashTreeMatchFinder counted{};
    LzssMatchFinderStatistics statistics{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, plain_storage.bytes, plain, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, counted_storage.bytes, counted, &statistics,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);

    for (std::size_t position = 0; position < input.size(); ++position) {
        EXPECT_EQ(plain.find_match(position), counted.find_match(position));
        plain.advance(position, position + 1U);
        counted.advance(position, position + 1U);
        ASSERT_TRUE(plain.state_valid()) << position;
        ASSERT_TRUE(counted.state_valid()) << position;
    }
    EXPECT_TRUE(std::ranges::equal(
        plain_storage.bytes.first(required.workspace_size),
        counted_storage.bytes.first(required.workspace_size)));
    EXPECT_EQ(statistics.hash_tree_chain_query_count
                  + statistics.hash_tree_tree_query_count,
              statistics.query_count);
    EXPECT_GT(statistics.hash_tree_chain_candidate_count, 0U);
    EXPECT_GT(statistics.hash_tree_trigger_query_count, 0U);
    EXPECT_EQ(statistics.hash_tree_trigger_query_count,
              statistics.hash_tree_promotion_count);
    EXPECT_GT(statistics.hash_tree_promotion_build_node_count, 0U);
    EXPECT_GT(statistics.hash_tree_tree_query_count, 0U);
    EXPECT_GE(statistics.hash_tree_tree_query_node_count,
              statistics.hash_tree_tree_query_count);
    EXPECT_GT(statistics.hash_tree_insertion_count, 0U);
    EXPECT_GT(statistics.hash_tree_tree_query_key_comparison_count, 0U);
    EXPECT_GT(statistics.hash_tree_tree_query_key_byte_comparison_count, 0U);
    EXPECT_GT(statistics.hash_tree_tree_query_lcp_byte_comparison_count, 0U);
    EXPECT_GT(
        statistics.hash_tree_tree_query_prefix_range_comparison_count, 0U);
    EXPECT_GT(
        statistics.hash_tree_tree_query_prefix_range_byte_comparison_count,
        0U);
    EXPECT_EQ(statistics.hash_tree_tree_query_lcp_skipped_byte_count, 0U);
    EXPECT_GT(statistics.hash_tree_maintenance_key_comparison_count, 0U);
    EXPECT_GT(statistics.hash_tree_maintenance_key_byte_comparison_count, 0U);
    EXPECT_GT(statistics.hash_tree_maximum_height, 0U);
    EXPECT_EQ(statistics.hash_tree_maximum_promoted_buckets,
              statistics.hash_tree_promotion_count);
    EXPECT_GT(statistics.hash_tree_maximum_promoted_nodes, 0U);
    std::uint64_t chain_histogram_total{};
    std::uint64_t tree_histogram_total{};
    for (const auto value :
         statistics.hash_tree_chain_query_depth_histogram) {
        chain_histogram_total += value;
    }
    for (const auto value :
         statistics.hash_tree_tree_query_depth_histogram) {
        tree_histogram_total += value;
    }
    EXPECT_EQ(chain_histogram_total,
              statistics.hash_tree_chain_query_count);
    EXPECT_EQ(tree_histogram_total,
              statistics.hash_tree_tree_query_count);
}

TEST(LzssHashTreeMatchFinder, PromotionBuildFailureDoesNotPublish) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    EXPECT_EQ(finder.find_match(0), LzssMatch{});
    finder.advance(0, 1);
    static_cast<void>(finder.find_match(1));

    auto links = mutable_array_at<std::uint32_t>(
        storage.bytes, required.link_offset, required.node_count);
    links[0] = 1;
    const auto roots_before = std::vector<std::uint32_t>(
        mutable_array_at<std::uint32_t>(
            storage.bytes, required.root_offset, required.bucket_count).begin(),
        mutable_array_at<std::uint32_t>(
            storage.bytes, required.root_offset, required.bucket_count).end());
    const auto modes_before = std::vector<LzssHashTreeBucketMode>(
        mutable_array_at<LzssHashTreeBucketMode>(
            storage.bytes, required.mode_offset, required.bucket_count).begin(),
        mutable_array_at<LzssHashTreeBucketMode>(
            storage.bytes, required.mode_offset, required.bucket_count).end());
    finder.advance(1, 2);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(), LzssHashTreeError::promotion_failure);
    const auto roots_after = mutable_array_at<std::uint32_t>(
        storage.bytes, required.root_offset, required.bucket_count);
    const auto modes_after = mutable_array_at<LzssHashTreeBucketMode>(
        storage.bytes, required.mode_offset, required.bucket_count);
    EXPECT_TRUE(std::ranges::equal(roots_after, roots_before));
    EXPECT_TRUE(std::ranges::equal(modes_after, modes_before));
}

TEST(LzssHashTreeMatchFinder, PublishedTreeFailureIsSticky) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    static_cast<void>(finder.find_match(0));
    finder.advance(0, 1);
    static_cast<void>(finder.find_match(1));
    finder.advance(1, 2);
    ASSERT_TRUE(finder.state_valid());

    const auto hash = calculate_lzss_prefix_hash(input, 2);
    ASSERT_TRUE(hash.valid);
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (required.bucket_count - 1U);
    const auto roots = mutable_array_at<std::uint32_t>(
        storage.bytes, required.root_offset, required.bucket_count);
    auto positions = mutable_array_at<LzssHashTreeStoredPosition>(
        storage.bytes, required.position_offset, required.node_count);
    positions[roots[bucket]] = 2;
    EXPECT_EQ(finder.find_match(2), LzssMatch{});
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(), LzssHashTreeError::tree_query_failure);
    const auto corrupted = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());
    finder.advance(2, 3);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, corrupted));
}

TEST(LzssHashTreeMatchFinder, PromotedModeSurvivesEmptyAndReactivation) {
    const auto input = bytes("AAAAAA0123456789AAAAAA");
    LzssParameters parameters{};
    parameters.window_size = 5;
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    LzssMatchFinderStatistics statistics{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder, &statistics,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    auto roots = mutable_array_at<std::uint32_t>(
        storage.bytes, required.root_offset, required.bucket_count);
    auto modes = mutable_array_at<LzssHashTreeBucketMode>(
        storage.bytes, required.mode_offset, required.bucket_count);
    const auto target_hash = calculate_lzss_prefix_hash(input, 0);
    ASSERT_TRUE(target_hash.valid);
    const auto target_bucket = static_cast<std::size_t>(target_hash.value)
        & (required.bucket_count - 1U);

    for (std::size_t position = 0; position < 7; ++position) {
        static_cast<void>(finder.find_match(position));
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
    }
    ASSERT_EQ(modes[target_bucket],
              LzssHashTreeBucketMode::promoted_tree);
    EXPECT_EQ(roots[target_bucket], lzss_hash_tree_null_node);

    for (std::size_t position = 7; position <= 16; ++position) {
        static_cast<void>(finder.find_match(position));
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
    }
    EXPECT_EQ(modes[target_bucket],
              LzssHashTreeBucketMode::promoted_tree);
    EXPECT_NE(roots[target_bucket], lzss_hash_tree_null_node);
    EXPECT_GT(statistics.hash_tree_retirement_count, 0U);
    EXPECT_GT(statistics.hash_tree_insertion_count, 0U);
    EXPECT_GT(statistics.hash_tree_maximum_promoted_buckets, 0U);
    EXPECT_GT(statistics.hash_tree_maximum_promoted_nodes, 0U);
}

TEST(LzssHashTreeMatchFinder, HashTreeStatisticsSaturate) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAA");
    LzssParameters parameters{};
    parameters.window_size = 5;
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, {});
    auto storage = make_storage(required.workspace_size);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    LzssMatchFinderStatistics statistics{};
    statistics.hash_tree_chain_query_count = maximum;
    statistics.hash_tree_chain_candidate_count = maximum;
    statistics.hash_tree_trigger_query_count = maximum;
    statistics.hash_tree_tree_query_count = maximum;
    statistics.hash_tree_promotion_count = maximum;
    statistics.hash_tree_promotion_trigger_candidate_count = maximum;
    statistics.hash_tree_promotion_build_node_count = maximum;
    statistics.hash_tree_tree_query_node_count = maximum;
    statistics.hash_tree_insertion_count = maximum;
    statistics.hash_tree_retirement_count = maximum;
    statistics.hash_tree_promotion_build_key_comparison_count = maximum;
    statistics.hash_tree_promotion_build_key_byte_comparison_count = maximum;
    statistics.hash_tree_promotion_build_rotation_count = maximum;
    statistics.hash_tree_tree_query_key_comparison_count = maximum;
    statistics.hash_tree_tree_query_key_byte_comparison_count = maximum;
    statistics.hash_tree_tree_query_lcp_byte_comparison_count = maximum;
    statistics.hash_tree_tree_query_prefix_range_comparison_count = maximum;
    statistics.hash_tree_tree_query_prefix_range_byte_comparison_count =
        maximum;
    statistics.hash_tree_tree_query_lcp_skipped_byte_count = maximum;
    statistics.hash_tree_maintenance_key_comparison_count = maximum;
    statistics.hash_tree_maintenance_key_byte_comparison_count = maximum;
    statistics.hash_tree_rotation_count = maximum;
    std::ranges::fill(
        statistics.hash_tree_chain_query_depth_histogram, maximum);
    std::ranges::fill(
        statistics.hash_tree_tree_query_depth_histogram, maximum);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, parameters, {}, storage.bytes, finder, &statistics,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);

    for (std::size_t position = 0; position < input.size(); ++position) {
        static_cast<void>(finder.find_match(position));
        finder.advance(position, position + 1U);
        ASSERT_TRUE(finder.state_valid()) << position;
    }
    EXPECT_TRUE(statistics.overflowed);
    EXPECT_EQ(statistics.hash_tree_chain_query_count, maximum);
    EXPECT_EQ(statistics.hash_tree_chain_candidate_count, maximum);
    EXPECT_EQ(statistics.hash_tree_trigger_query_count, maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_count, maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_count, maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_trigger_candidate_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_build_node_count, maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_node_count, maximum);
    EXPECT_EQ(statistics.hash_tree_insertion_count, maximum);
    EXPECT_EQ(statistics.hash_tree_retirement_count, maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_build_key_comparison_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_build_key_byte_comparison_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_promotion_build_rotation_count, maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_key_comparison_count, maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_key_byte_comparison_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_lcp_byte_comparison_count,
              maximum);
    EXPECT_EQ(
        statistics.hash_tree_tree_query_prefix_range_comparison_count,
        maximum);
    EXPECT_EQ(
        statistics.hash_tree_tree_query_prefix_range_byte_comparison_count,
        maximum);
    EXPECT_EQ(statistics.hash_tree_tree_query_lcp_skipped_byte_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_maintenance_key_comparison_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_maintenance_key_byte_comparison_count,
              maximum);
    EXPECT_EQ(statistics.hash_tree_rotation_count, maximum);
    EXPECT_TRUE(std::ranges::all_of(
        statistics.hash_tree_chain_query_depth_histogram,
        [maximum](const auto value) { return value == maximum; }));
    EXPECT_TRUE(std::ranges::all_of(
        statistics.hash_tree_tree_query_depth_histogram,
        [maximum](const auto value) { return value == maximum; }));
}

TEST(LzssHashTreeMatchFinder, TreeMutationFailureIsSticky) {
    const auto input = bytes("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), {}, {});
    auto storage = make_storage(required.workspace_size);
    LzssHashTreeMatchFinder finder{};
    ASSERT_EQ(initialize_lzss_hash_tree_match_finder(
                  input, {}, {}, storage.bytes, finder, nullptr,
                  LzssHashTreeOptions{0}),
              LzssHashTreeError::none);
    static_cast<void>(finder.find_match(0));
    finder.advance(0, 1);
    static_cast<void>(finder.find_match(1));
    finder.advance(1, 2);
    ASSERT_TRUE(finder.state_valid());

    const auto hash = calculate_lzss_prefix_hash(input, 2);
    ASSERT_TRUE(hash.valid);
    const auto bucket = static_cast<std::size_t>(hash.value)
        & (required.bucket_count - 1U);
    const auto roots = mutable_array_at<std::uint32_t>(
        storage.bytes, required.root_offset, required.bucket_count);
    static_cast<void>(finder.find_match(2));
    ASSERT_TRUE(finder.state_valid());
    auto right = mutable_array_at<std::uint32_t>(
        storage.bytes, required.right_offset, required.node_count);
    right[roots[bucket]] = roots[bucket];
    finder.advance(2, 3);
    EXPECT_FALSE(finder.state_valid());
    EXPECT_EQ(finder.last_error(),
              LzssHashTreeError::tree_mutation_failure);
    const auto corrupted = std::vector<std::byte>(
        storage.bytes.begin(), storage.bytes.end());
    finder.advance(3, 4);
    EXPECT_TRUE(std::ranges::equal(storage.bytes, corrupted));
}

} // namespace

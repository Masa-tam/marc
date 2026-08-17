#include "dictionary/lzss_hash_tree_match_finder.hpp"

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

void expect_all_bytes(
    const std::span<const std::byte> values,
    const std::byte expected) {
    EXPECT_TRUE(std::ranges::all_of(values, [expected](const auto value) {
        return value == expected;
    }));
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
    EXPECT_EQ(required.link_offset, 8U * sizeof(std::size_t));
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
              align_size(required.height_offset + 5U, alignof(std::size_t)));
    EXPECT_EQ(required.subtree_maximum_position_offset,
              required.position_offset + 5U * sizeof(std::size_t));
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
              (22U + 3U * sizeof(std::size_t))
                  * static_cast<std::size_t>(65'536));

    LzssParameters parameters{};
    parameters.window_size = 1U << 20;
    required = calculate_lzss_hash_tree_workspace(
        1U << 20, parameters, {});
    ASSERT_EQ(required.error, LzssHashTreeError::none);
    EXPECT_EQ(required.bucket_count, 65'536U);
    EXPECT_EQ(required.node_count, 1U << 20);
    const auto expected = static_cast<std::size_t>(1U << 20)
            * (17U + 2U * sizeof(std::size_t))
        + static_cast<std::size_t>(65'536)
            * (5U + sizeof(std::size_t));
    EXPECT_EQ(required.workspace_size, expected);
    if constexpr (sizeof(std::size_t) == 8) {
        EXPECT_EQ(required.workspace_size, 35'454'976U);
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
                sizeof(std::size_t), alignof(std::size_t)},
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
                sizeof(std::size_t), alignof(std::size_t)},
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
    const auto heads = array_at<std::size_t>(
        active, required.head_offset, required.bucket_count);
    EXPECT_TRUE(std::ranges::all_of(heads, [](const auto value) {
        return value == lzss_hash_tree_no_position;
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

} // namespace

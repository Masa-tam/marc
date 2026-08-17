#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {
using namespace marc::dictionary::internal;

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

} // namespace

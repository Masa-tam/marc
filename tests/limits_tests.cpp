#include "core/limits.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace {

[[nodiscard]] marc::core::FrameBounds valid_frame() {
    marc::core::FrameBounds frame{};
    frame.uncompressed_size = UINT64_C(1) << 20;
    frame.dictionary_serialized_size = UINT64_C(1) << 20;
    frame.compressed_payload_size = 1023;
    frame.largest_block_size = UINT64_C(1) << 16;
    frame.dictionary_entries = UINT64_C(1) << 12;
    frame.lz_distance = UINT64_C(1) << 15;
    frame.lz_match_length = UINT64_C(1) << 10;
    frame.huffman_code_length = 15;
    frame.entropy_table_entries = UINT64_C(1) << 12;
    frame.range_model_total = UINT64_C(1) << 15;
    frame.model_buffered_bytes = UINT64_C(1) << 16;
    frame.payload_buffered_bytes = UINT64_C(1) << 20;
    frame.block_count = 16;
    return frame;
}

} // namespace

using marc::core::DecoderLimits;
using marc::core::LimitError;

TEST(DecoderLimitsTest, AcceptsDefaults) {
    EXPECT_EQ(marc::core::validate_limits(DecoderLimits{}), LimitError::none);
}

TEST(DecoderLimitsTest, RejectsImpossibleHuffmanLength) {
    DecoderLimits limits{};
    limits.max_huffman_code_length = 65;
    EXPECT_EQ(marc::core::validate_limits(limits),
              LimitError::invalid_configuration);
}

TEST(FrameBoundsTest, AcceptsFrameWithinDefaults) {
    EXPECT_EQ(marc::core::validate_frame_bounds(
                  DecoderLimits{}, valid_frame(), 0),
              LimitError::none);
}

TEST(FrameBoundsTest, RejectsOversizedFrame) {
    const DecoderLimits limits{};
    auto frame = valid_frame();
    frame.uncompressed_size = limits.max_frame_size + 1;
    EXPECT_EQ(marc::core::validate_frame_bounds(limits, frame, 0),
              LimitError::frame_size);
}

TEST(FrameBoundsTest, RejectsOversizedBlock) {
    const DecoderLimits limits{};
    auto frame = valid_frame();
    frame.largest_block_size = limits.max_block_size + 1;
    EXPECT_EQ(marc::core::validate_frame_bounds(limits, frame, 0),
              LimitError::block_size);
}

TEST(FrameBoundsTest, RejectsOversizedDictionarySerialization) {
    const DecoderLimits limits{};
    auto frame = valid_frame();
    frame.dictionary_serialized_size =
        limits.max_dictionary_serialized_size + 1;
    EXPECT_EQ(marc::core::validate_frame_bounds(limits, frame, 0),
              LimitError::dictionary_serialized_size);
}

TEST(FrameBoundsTest, RejectsBufferedSizeOverflow) {
    const DecoderLimits limits{};
    auto frame = valid_frame();
    frame.model_buffered_bytes = std::numeric_limits<std::uint64_t>::max();
    frame.payload_buffered_bytes = 1;
    EXPECT_EQ(marc::core::validate_frame_bounds(limits, frame, 0),
              LimitError::arithmetic_overflow);
}

TEST(FrameBoundsTest, RejectsExcessiveExpansion) {
    const DecoderLimits limits{};
    auto frame = valid_frame();
    frame.compressed_payload_size = 0;
    frame.uncompressed_size = limits.expansion_slack + 1;
    EXPECT_EQ(marc::core::validate_frame_bounds(limits, frame, 0),
              LimitError::expansion_ratio);
}

TEST(FrameBoundsTest, RejectsCumulativeOutputOverflow) {
    EXPECT_EQ(marc::core::validate_frame_bounds(
                  DecoderLimits{}, valid_frame(),
                  std::numeric_limits<std::uint64_t>::max()),
              LimitError::arithmetic_overflow);
}

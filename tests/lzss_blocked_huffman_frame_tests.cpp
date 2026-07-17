#include "frame/lzss_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::LzssBlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = 1;
    return stream;
}

constexpr std::array<std::byte, 74> single_literal_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0x41}};

} // namespace

TEST(LzssBlockedHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging);
    ASSERT_EQ(result.error,
              LzssBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.dictionary_size, 2U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[1], std::byte{0x41});
}

TEST(LzssBlockedHuffmanFrameValidator,
     StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
            stream_for_a(), {}, {}, 0, 0,
            std::span<const std::byte>{single_literal_frame}.first(size),
            views, staging);
        EXPECT_NE(result.error,
                  LzssBlockedHuffmanFrameValidationError::none) << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzssBlockedHuffmanFrameValidator,
     RejectsWorkspaceCapacityBeforeWritingStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 1> short_staging{std::byte{0x5a}};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  views, short_staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_EQ(short_staging[0], std::byte{0x5a});

    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  std::span<marc::entropy::internal::
                      BlockedHuffmanBlockView>{},
                  staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::view_output_too_small);
}

TEST(LzssBlockedHuffmanFrameValidator, EnforcesAggregateWorkspaceLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 2;
    const std::uint64_t required = 16 + 2 + 2
        + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(LzssBlockedHuffmanFrameValidator,
     RejectsEntropyMetadataBeforeWritingStaging) {
    auto malformed = single_literal_frame;
    malformed[64] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error,
              LzssBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(LzssBlockedHuffmanFrameValidator, RejectsInvalidStagedLzssToken) {
    auto malformed = single_literal_frame;
    malformed[72] = std::byte{0xff};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    const auto result = marc::frame::validate_lzss_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error, LzssBlockedHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::LzssValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(LzssBlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 2> staging{};
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzss_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, views, staging)
                  .error,
              LzssBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

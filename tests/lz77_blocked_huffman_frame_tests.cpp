#include "frame/lz77_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::frame::Lz77BlockedHuffmanFrameValidationError;

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.entropy_block_size = 16;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = 1;
    return stream;
}

constexpr std::array<std::byte, 88> single_literal_frame{
    std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0}};

} // namespace

TEST(Lz77BlockedHuffmanFrameValidator, AcceptsHandVectorIntoStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, single_literal_frame, views, staging);
    ASSERT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, single_literal_frame.size());
    EXPECT_EQ(result.dictionary_size, 16U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(staging[0], std::byte{0});
    EXPECT_EQ(staging[12], std::byte{0x41});
}

TEST(Lz77BlockedHuffmanFrameValidator, StrictlyRejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    for (std::size_t size = 0; size < single_literal_frame.size(); ++size) {
        const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
            stream_for_a(), {}, {}, 0, 0,
            std::span<const std::byte>{single_literal_frame}.first(size),
            views, staging);
        EXPECT_NE(result.error, Lz77BlockedHuffmanFrameValidationError::none)
            << size;
    }
    std::vector<std::byte> extended(single_literal_frame.begin(),
                                    single_literal_frame.end());
    extended.push_back(std::byte{0});
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, extended, views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsWorkspaceCapacityBeforeWritingStaging) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 15> short_staging{};
    short_staging.fill(std::byte{0x5a});
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  views, short_staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 0, 0, single_literal_frame,
                  std::span<marc::entropy::internal::BlockedHuffmanBlockView>{},
                  staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::
                  view_output_too_small);
}

TEST(Lz77BlockedHuffmanFrameValidator, EnforcesAggregateWorkspaceLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    const std::uint64_t required = 16 + 16 + 16
        + sizeof(marc::entropy::internal::BlockedHuffmanBlockView);
    limits.max_internal_buffered_bytes = required - 1;
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, limits, 0, 0, single_literal_frame,
                  views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::workspace_limit);
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsEntropyMetadataBeforeStaging) {
    auto malformed = single_literal_frame;
    malformed[64] = std::byte{1};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    staging.fill(std::byte{0x5a});
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error,
              Lz77BlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsInvalidStagedLz77Token) {
    auto malformed = single_literal_frame;
    malformed[72] = std::byte{0xff};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    const auto result = marc::frame::validate_lz77_blocked_huffman_frame(
        stream_for_a(), {}, {}, 0, 0, malformed, views, staging);
    EXPECT_EQ(result.error, Lz77BlockedHuffmanFrameValidationError::
                                dictionary_validation_error);
    EXPECT_EQ(result.dictionary_error,
              marc::dictionary::internal::Lz77ValidationError::token_error);
    EXPECT_EQ(staging[0], std::byte{0xff});
}

TEST(Lz77BlockedHuffmanFrameValidator, RejectsWrongSequenceAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 16> staging{};
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream_for_a(), {}, {}, 1, 0, single_literal_frame,
                  views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::header_error);
    auto stream = stream_for_a();
    stream.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lz77_blocked_huffman_frame(
                  stream, {}, {}, 0, 0, single_literal_frame, views, staging)
                  .error,
              Lz77BlockedHuffmanFrameValidationError::unsupported_pipeline);
}

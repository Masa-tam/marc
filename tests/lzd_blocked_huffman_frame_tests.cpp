#include "frame/lzd_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

namespace {

using marc::frame::LzdBlockedHuffmanFrameValidationError;

constexpr std::array single_terminal_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{16}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{8},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};

marc::frame::StreamHeader stream(const std::uint64_t size = 1) {
    marc::frame::StreamHeader result{};
    result.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzd;
    result.dictionary_variant = 1;
    result.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    result.entropy_variant = 1;
    result.entropy_block_size = 8;
    result.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    result.original_size = size;
    return result;
}

TEST(LzdBlockedHuffmanFrameValidator, AcceptsSpecifiedHandVector) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    const auto result = marc::frame::validate_lzd_blocked_huffman_frame(
        stream(), {}, {}, 0, 0, single_terminal_frame, views, staging, {});
    ASSERT_EQ(result.error, LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(result.serialized_size, 80U);
    EXPECT_EQ(result.dictionary_size, 8U);
    EXPECT_EQ(result.raw_size, 1U);
    EXPECT_EQ(result.block_count, 1U);
    EXPECT_EQ(result.phrase_entries, 0U);
    EXPECT_EQ(result.dictionary_entries, 0U);
    EXPECT_EQ(result.expansion_entries, 1U);
    EXPECT_EQ(staging[0], std::byte{'A'});
    EXPECT_EQ(staging[4], std::byte{0xff});
}

TEST(LzdBlockedHuffmanFrameValidator, RejectsEveryTruncationAndTrailingData) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    for (std::size_t size = 0; size < single_terminal_frame.size(); ++size) {
        EXPECT_NE(marc::frame::validate_lzd_blocked_huffman_frame(
                      stream(), {}, {}, 0, 0,
                      std::span<const std::byte>{single_terminal_frame}.first(
                          size),
                      views, staging, {})
                      .error,
                  LzdBlockedHuffmanFrameValidationError::none)
            << size;
    }
    auto trailing = single_terminal_frame;
    std::array<std::byte, single_terminal_frame.size() + 1> extended{};
    std::ranges::copy(trailing, extended.begin());
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, extended, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::trailing_frame_bytes);
}

TEST(LzdBlockedHuffmanFrameValidator, RejectsCallerWorkspaceShortages) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, {}, staging,
                  {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::view_output_too_small);
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  std::span<std::byte>{}, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  dictionary_staging_too_small);

    auto pair = single_terminal_frame;
    pair[16] = std::byte{2};
    pair[76] = std::byte{'B'};
    pair[77] = pair[78] = pair[79] = std::byte{};
    auto pair_stream = stream(2);
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  pair_stream, {}, {}, 0, 0, pair, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  phrase_workspace_too_small);
}

TEST(LzdBlockedHuffmanFrameValidator, EntropyThenDictionaryFailuresAreAtomic) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    staging.fill(std::byte{0x5a});
    auto bad_descriptor = single_terminal_frame;
    bad_descriptor[56] = std::byte{7};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_descriptor, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::controller_error);
    EXPECT_TRUE(std::ranges::all_of(staging, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));

    auto bad_reference = single_terminal_frame;
    bad_reference[72] = std::byte{0};
    bad_reference[73] = std::byte{1};
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, bad_reference, views, staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  dictionary_validation_error);
}

TEST(LzdBlockedHuffmanFrameDecoder, PublishesOnlyAfterCompleteValidation) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    ASSERT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::none);
    EXPECT_EQ(output[0], std::byte{'A'});

    output[0] = std::byte{0x5a};
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, {}, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::
                  expansion_workspace_too_small);
    EXPECT_EQ(output[0], std::byte{0x5a});
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::raw_output_too_small);
}

TEST(LzdBlockedHuffmanFrameDecoder, EnforcesCompleteAggregateAndPipeline) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    std::array<std::byte, 8> staging{};
    std::array<std::uint32_t, 1> expansion{};
    std::array<std::byte, 1> output{std::byte{0x5a}};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 8;
    limits.max_internal_buffered_bytes = 16 + 8 + 8 + sizeof(views)
        + sizeof(expansion) + output.size() - 1;
    EXPECT_EQ(marc::frame::decode_lzd_blocked_huffman_frame(
                  stream(), {}, limits, 0, 0, single_terminal_frame, views,
                  staging, {}, expansion, output)
                  .error,
              LzdBlockedHuffmanFrameValidationError::workspace_limit);
    EXPECT_EQ(output[0], std::byte{0x5a});

    auto unsupported = stream();
    unsupported.dictionary_variant = 0;
    EXPECT_EQ(marc::frame::validate_lzd_blocked_huffman_frame(
                  unsupported, {}, {}, 0, 0, single_terminal_frame, views,
                  staging, {})
                  .error,
              LzdBlockedHuffmanFrameValidationError::unsupported_pipeline);
}

} // namespace

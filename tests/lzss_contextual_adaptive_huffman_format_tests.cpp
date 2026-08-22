#include "frame/lzss_contextual_adaptive_huffman_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::entropy::internal::ContextualAdaptiveHuffmanFormatError;

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config() {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader selected_stream_config(
    const std::uint64_t size) {
    auto stream = stream_config();
    stream.frame_size = static_cast<std::uint32_t>(size);
    stream.original_size = size;
    stream.dictionary.window_size = 1'048'576;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    return stream;
}

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader four_mib_stream_config(
    const std::uint64_t size) {
    auto stream = stream_config();
    stream.frame_size = static_cast<std::uint32_t>(size);
    stream.original_size = size;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> frame(82);
    const auto stream = stream_config();
    const LzssContextualAdaptiveHuffmanFrameHeader header{
        0, 0, 1, 1, 2, 2, 2, 16, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, frame.begin() + 64);
    frame[80] = std::byte{0x82};
    frame[81] = std::byte{0x00};
    return frame;
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanFormat,
     StreamHeaderMatchesReservedIdentityAndRoundTrips) {
    const auto stream = stream_config();
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream, {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(bytes[16], std::byte{1});
    EXPECT_EQ(bytes[18], std::byte{2});
    constexpr std::array entropy{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x02},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x08}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        entropy, std::span<const std::byte>{bytes}.subspan<80, 16>()));

    LzssContextualAdaptiveHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, bytes.size());
    EXPECT_EQ(parsed.frame_size, 64U);
    EXPECT_EQ(parsed.original_size, 1U);
    EXPECT_EQ(parsed.max_symbol_events, UINT32_C(1) << 25);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     StreamFailuresAreTransactionalAndBounded) {
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream_config(), {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    LzssContextualAdaptiveHuffmanStreamHeader parsed{};
    parsed.frame_size = 0xCCCCCCCCU;
    std::size_t consumed = 0xCCCCU;
    bytes[16] = std::byte{2};
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  unknown_entropy_algorithm);
    EXPECT_EQ(parsed.frame_size, 0xCCCCCCCCU);
    EXPECT_EQ(consumed, 0xCCCCU);

    bytes[16] = std::byte{1};
    bytes[18] = std::byte{1};
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  unsupported_entropy_variant);
    bytes[18] = std::byte{2};
    bytes[88] = std::byte{1};
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  nonzero_reserved);
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_adaptive_huffman_node_entries
        + marc::entropy::internal::contextual_adaptive_huffman_symbol_entries
        - 1;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  stream_config(), limits),
              LzssContextualAdaptiveHuffmanStreamHeaderError::limit_exceeded);

    auto oversized = stream_config();
    oversized.frame_size =
        lzss_contextual_adaptive_huffman_max_frame_size + 1;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  oversized, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::limit_exceeded);

    auto invalid_dictionary = stream_config();
    invalid_dictionary.dictionary.min_match_length = 4;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  invalid_dictionary, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  invalid_dictionary_parameters);
    auto invalid_entropy = stream_config();
    invalid_entropy.max_nyt_raw_width = 7;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  invalid_entropy, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  invalid_entropy_parameters);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     SelectedStreamIdentityRoundTripsAndRejectsCrossedPairs) {
    const auto stream = selected_stream_config(5);
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream, {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(bytes[14], std::byte{3});
    EXPECT_EQ(bytes[16], std::byte{1});
    EXPECT_EQ(bytes[18], std::byte{2});
    EXPECT_EQ(bytes[96], std::byte{1});
    EXPECT_EQ(bytes[98], std::byte{2});

    LzssContextualAdaptiveHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, bytes.size());
    EXPECT_EQ(parsed.dictionary.window_size, 1'048'576U);
    EXPECT_EQ(parsed.dictionary_variant, 3U);
    EXPECT_EQ(parsed.context_algorithm, 1U);
    EXPECT_EQ(parsed.context_variant, 2U);

    auto crossed = stream;
    crossed.context_variant = 1;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  crossed, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  crossed, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  contradictory_parameters);

    const LzssContextualAdaptiveHuffmanFrameHeader extended_counts{
        0, 0, 5, 1, 2, 27, 1, 16, 0, 0};
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  extended_counts, {stream, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    auto legacy = stream_config();
    legacy.frame_size = 5;
    legacy.original_size = 5;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  extended_counts, {legacy, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::
                  contradictory_counts);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     FourMiBIdentityRoundTripsAndSelectsSevenFBound) {
    const auto stream = four_mib_stream_config(5);
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream, {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(bytes[14], std::byte{4});
    EXPECT_EQ(bytes[16], std::byte{1});
    EXPECT_EQ(bytes[18], std::byte{2});
    EXPECT_EQ(bytes[96], std::byte{1});
    EXPECT_EQ(bytes[98], std::byte{3});

    LzssContextualAdaptiveHuffmanStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    EXPECT_EQ(consumed, bytes.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 22);
    EXPECT_EQ(parsed.dictionary_variant, 4U);
    EXPECT_EQ(parsed.context_variant, 3U);

    auto crossed = stream;
    crossed.context_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  crossed, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_stream_header(
                  crossed, {}),
              LzssContextualAdaptiveHuffmanStreamHeaderError::
                  contradictory_parameters);

    LzssContextualAdaptiveHuffmanFrameHeader header{
        0, 0, 5, 2, 4, 31, 1, 16, 0, 0};
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    auto one_mib = selected_stream_config(5);
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  header, {one_mib, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::
                  contradictory_counts);

    header.decision_count = 35;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    ++header.decision_count;
    EXPECT_EQ(validate_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, 0, 0}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::
                  contradictory_counts);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     RejectsEveryTruncatedStreamHeaderPrefix) {
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream_config(), {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        SCOPED_TRACE(size);
        LzssContextualAdaptiveHuffmanStreamHeader parsed{};
        parsed.frame_size = 0xCCCCCCCCU;
        std::size_t consumed = 0xCCCCU;
        EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_stream_header(
                      std::span<const std::byte>{bytes}.first(size), {},
                      parsed, consumed),
                  LzssContextualAdaptiveHuffmanStreamHeaderError::
                      truncated_header);
        EXPECT_EQ(parsed.frame_size, 0xCCCCCCCCU);
        EXPECT_EQ(consumed, 0xCCCCU);
    }
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     PreflightsDocumentedFrameAndLeavesTrailingBytes) {
    auto frame = frame_vector();
    frame.push_back(std::byte{0xA5});
    const auto stream = stream_config();
    LzssContextualAdaptiveHuffmanFrameLayout layout{};
    const auto result = preflight_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, layout);
    ASSERT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, 82U);
    EXPECT_EQ(layout.header.descriptor_size, 16U);
    EXPECT_EQ(layout.header.payload_size, 2U);
    EXPECT_EQ(layout.descriptor.decision_count, 2U);
    EXPECT_EQ(layout.descriptor.final_valid_bits, 1U);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     RejectsEveryTruncatedPrefixTransactionally) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    for (std::size_t size = 0; size < frame.size(); ++size) {
        SCOPED_TRACE(size);
        LzssContextualAdaptiveHuffmanFrameLayout layout{};
        layout.serialized_size = 0xCCCCU;
        const auto result = preflight_lzss_contextual_adaptive_huffman_frame(
            std::span<const std::byte>{frame}.first(size),
            {stream, {}, 0, 0}, layout);
        EXPECT_NE(result.error,
                  LzssContextualAdaptiveHuffmanFramePreflightError::none);
        EXPECT_EQ(layout.serialized_size, 0xCCCCU);
    }
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     FrameHeaderRoundTripsAndRejectsMetadataAtomically) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    LzssContextualAdaptiveHuffmanFrameHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_adaptive_huffman_frame_header(
                  frame, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    EXPECT_EQ(consumed, 64U);
    std::array<std::byte, 64> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  parsed, {stream, {}, 0, 0}, encoded),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    EXPECT_TRUE(std::ranges::equal(
        encoded, std::span<const std::byte>{frame}.first(64)));

    auto mutated = frame;
    mutated[8] = std::byte{1};
    parsed.uncompressed_size = 0xCCCCCCCCU;
    consumed = 0xCCCCU;
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_frame_header(
                  mutated, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualAdaptiveHuffmanFrameHeaderError::
                  unexpected_sequence);
    EXPECT_EQ(parsed.uncompressed_size, 0xCCCCCCCCU);
    EXPECT_EQ(consumed, 0xCCCCU);

    mutated = frame;
    mutated[48] = std::byte{1};
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_frame_header(
                  mutated, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualAdaptiveHuffmanFrameHeaderError::nonzero_reserved);
    mutated = frame;
    mutated[40] = std::byte{1};
    EXPECT_EQ(parse_lzss_contextual_adaptive_huffman_frame_header(
                  mutated, {stream, {}, 0, 0}, parsed, consumed),
              LzssContextualAdaptiveHuffmanFrameHeaderError::
                  unsupported_feature);
}

TEST(LzssContextualAdaptiveHuffmanFormat,
     RejectsHeaderDescriptorAndLimitContradictions) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    LzssContextualAdaptiveHuffmanFrameLayout layout{};
    frame[28] = std::byte{3};
    auto result = preflight_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFramePreflightError::
                  descriptor_error);
    EXPECT_EQ(result.descriptor_error,
              ContextualAdaptiveHuffmanFormatError::contradictory_size);

    frame = frame_vector();
    frame[74] = std::byte{9};
    result = preflight_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFramePreflightError::
                  descriptor_error);
    EXPECT_EQ(result.descriptor_error,
              ContextualAdaptiveHuffmanFormatError::invalid_final_bits);

    frame = frame_vector();
    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 1;
    result = preflight_lzss_contextual_adaptive_huffman_frame(
        frame, {stream, limits, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualAdaptiveHuffmanFramePreflightError::header_error);
    EXPECT_EQ(result.header_error,
              LzssContextualAdaptiveHuffmanFrameHeaderError::limit_exceeded);
}

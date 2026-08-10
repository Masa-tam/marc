#include "frame/lzss_contextual_tans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::entropy::internal::ContextualTansFormatError;

[[nodiscard]] LzssContextualTansStreamHeader stream_config() {
    LzssContextualTansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> bytes(96);
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x02};
    bytes[36] = std::byte{0x1e};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

} // namespace

TEST(LzssContextualTansFormat, StreamHeaderRoundTripsDistinctIdentity) {
    const auto stream = stream_config();
    std::array<std::byte, lzss_contextual_tans_stream_header_size> bytes{};
    ASSERT_EQ(serialize_lzss_contextual_tans_stream_header(
                  stream, {}, bytes),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(bytes[16], std::byte{5});
    EXPECT_EQ(bytes[18], std::byte{2});
    LzssContextualTansStreamHeader parsed{};
    std::size_t consumed{};
    EXPECT_EQ(parse_lzss_contextual_tans_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(consumed, bytes.size());
    EXPECT_EQ(parsed.frame_size, stream.frame_size);
    EXPECT_EQ(parsed.original_size, stream.original_size);

    bytes[16] = std::byte{4};
    EXPECT_EQ(parse_lzss_contextual_tans_stream_header(
                  bytes, {}, parsed, consumed),
              LzssContextualTansStreamHeaderError::unknown_entropy_algorithm);
}

TEST(LzssContextualTansFormat, PreflightsDocumentedOneLiteralFrame) {
    const auto bytes = frame_vector();
    const auto stream = stream_config();
    LzssContextualTansFrameLayout layout{};
    const auto result = preflight_lzss_contextual_tans_frame(
        bytes, {stream, {}, 0, 0}, layout);
    ASSERT_EQ(result.error, LzssContextualTansFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, bytes.size());
    EXPECT_EQ(layout.header.descriptor_size, 30U);
    EXPECT_EQ(layout.header.payload_size, 2U);
    EXPECT_EQ(layout.descriptor.decision_count, 2U);
    EXPECT_EQ(layout.descriptor.frequencies[0], 4096U);
    EXPECT_EQ(layout.descriptor.frequencies[71], 4096U);

    std::array<std::byte, lzss_contextual_tans_frame_header_size> header{};
    EXPECT_EQ(serialize_lzss_contextual_tans_frame_header(
                  layout.header, {stream, {}, 0, 0}, header),
              LzssContextualTansFrameHeaderError::none);
    EXPECT_TRUE(std::ranges::equal(
        header, std::span<const std::byte>{bytes}.first(header.size())));
}

TEST(LzssContextualTansFormat, RejectsDescriptorAndTruncationAtomically) {
    auto bytes = frame_vector();
    const auto stream = stream_config();
    LzssContextualTansFrameLayout layout{};
    layout.serialized_size = 0xa5a5;
    bytes[84] = std::byte{1};
    bytes[85] = std::byte{0};
    bytes[86] = std::byte{0};
    auto result = preflight_lzss_contextual_tans_frame(
        bytes, {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualTansFramePreflightError::descriptor_error);
    EXPECT_EQ(result.descriptor_error,
              ContextualTansFormatError::trailing_data);
    EXPECT_EQ(layout.serialized_size, 0xa5a5U);

    bytes = frame_vector();
    for (std::size_t prefix_size = 0; prefix_size < bytes.size();
         ++prefix_size) {
        SCOPED_TRACE(prefix_size);
        layout.serialized_size = 0xa5a5;
        result = preflight_lzss_contextual_tans_frame(
            std::span<const std::byte>{bytes}.first(prefix_size),
            {stream, {}, 0, 0}, layout);
        EXPECT_NE(result.error,
                  LzssContextualTansFramePreflightError::none);
        EXPECT_EQ(layout.serialized_size, 0xa5a5U);
    }
}

TEST(LzssContextualTansFormat, RejectsHeaderBoundsAndLimits) {
    auto bytes = frame_vector();
    const auto stream = stream_config();
    LzssContextualTansFrameLayout layout{};
    bytes[36] = std::byte{26};
    auto result = preflight_lzss_contextual_tans_frame(
        bytes, {stream, {}, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualTansFramePreflightError::header_error);
    EXPECT_EQ(result.header_error,
              LzssContextualTansFrameHeaderError::contradictory_counts);

    bytes = frame_vector();
    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = 131071;
    result = preflight_lzss_contextual_tans_frame(
        bytes, {stream, limits, 0, 0}, layout);
    EXPECT_EQ(result.error,
              LzssContextualTansFramePreflightError::header_error);
    EXPECT_EQ(result.header_error,
              LzssContextualTansFrameHeaderError::invalid_stream_header);
}

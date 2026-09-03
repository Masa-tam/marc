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

[[nodiscard]] LzssContextualTansStreamHeader stream_config_1m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 20;
    stream.dictionary.window_size = UINT32_C(1) << 20;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    stream.frequency_entry_count = 4550;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_config_4m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 22;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    stream.frequency_entry_count = 4566;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_config_16m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 24;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    stream.frequency_entry_count = 4582;
    return stream;
}

[[nodiscard]] LzssContextualTansStreamHeader stream_config_64m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 26;
    stream.dictionary.window_size = UINT32_C(1) << 26;
    stream.dictionary_variant = 6;
    stream.context_variant = 5;
    stream.frequency_entry_count = 4598;
    return stream;
}

[[nodiscard]] marc::core::DecoderLimits limits_64m() {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = UINT64_C(1) << 26;
    limits.max_block_size = UINT64_C(1) << 26;
    limits.max_lz_distance = UINT64_C(1) << 26;
    return limits;
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

TEST(LzssContextualTansFormat,
     RoundTripsSelectedOneMiBIdentityAndRejectsCrossedPairs) {
    const auto stream = stream_config_1m();
    std::array<std::byte, lzss_contextual_tans_stream_header_size> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_tans_stream_header(
                  stream, {}, encoded),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{0x03});
    EXPECT_EQ(encoded[16], std::byte{0x05});
    EXPECT_EQ(encoded[18], std::byte{0x02});
    EXPECT_EQ(encoded[98], std::byte{0x02});
    EXPECT_EQ(encoded[84], std::byte{0xc6});
    EXPECT_EQ(encoded[85], std::byte{0x11});

    LzssContextualTansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_tans_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 20);
    EXPECT_EQ(parsed.dictionary_variant, 3U);
    EXPECT_EQ(parsed.context_variant, 2U);
    EXPECT_EQ(parsed.frequency_entry_count, 4550U);

    auto crossed = stream;
    crossed.context_variant = 1;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.frequency_entry_count = 4518;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::invalid_entropy_parameters);
}

TEST(LzssContextualTansFormat,
     RoundTripsFourMiBIdentityAndSelectsSevenFBound) {
    const auto stream = stream_config_4m();
    std::array<std::byte, lzss_contextual_tans_stream_header_size> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_tans_stream_header(
                  stream, {}, encoded),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{0x04});
    EXPECT_EQ(encoded[16], std::byte{0x05});
    EXPECT_EQ(encoded[18], std::byte{0x02});
    EXPECT_EQ(encoded[98], std::byte{0x03});
    EXPECT_EQ(encoded[84], std::byte{0xd6});
    EXPECT_EQ(encoded[85], std::byte{0x11});

    LzssContextualTansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_tans_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 22);
    EXPECT_EQ(parsed.dictionary_variant, 4U);
    EXPECT_EQ(parsed.context_variant, 3U);
    EXPECT_EQ(parsed.frequency_entry_count, 4566U);

    auto crossed = stream;
    crossed.context_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);

    auto small = stream_config();
    small.frame_size = 5;
    small.original_size = 5;
    LzssContextualTansFrameHeader header{
        0, 0, 5, 1, 5, 32, 2,
        static_cast<std::uint32_t>(
            marc::entropy::internal::contextual_tans_min_descriptor_size),
        0, 0};
    EXPECT_EQ(validate_lzss_contextual_tans_frame_header(
                  header, {small, {}, 0, 0}),
              LzssContextualTansFrameHeaderError::contradictory_counts);
    auto selected = stream;
    selected.frame_size = 5;
    selected.original_size = 5;
    EXPECT_EQ(validate_lzss_contextual_tans_frame_header(
                  header, {selected, {}, 0, 0}),
              LzssContextualTansFrameHeaderError::none);
}

TEST(LzssContextualTansFormat,
     RoundTripsSixteenMiBIdentityAndRejectsCrossedPairs) {
    const auto stream = stream_config_16m();
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(stream, {}),
              LzssContextualTansStreamHeaderError::none);

    std::array<std::byte, lzss_contextual_tans_stream_header_size> output{};
    ASSERT_EQ(serialize_lzss_contextual_tans_stream_header(
                  stream, {}, output),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(output[14], std::byte{0x05});
    EXPECT_EQ(output[16], std::byte{0x05});
    EXPECT_EQ(output[18], std::byte{0x02});
    EXPECT_EQ(output[98], std::byte{0x04});
    EXPECT_EQ(output[84], std::byte{0xe6});
    EXPECT_EQ(output[85], std::byte{0x11});

    LzssContextualTansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_tans_stream_header(
                  output, {}, parsed, consumed),
              LzssContextualTansStreamHeaderError::none);
    EXPECT_EQ(consumed, output.size());
    EXPECT_EQ(parsed.dictionary_variant, 5U);
    EXPECT_EQ(parsed.context_variant, 4U);
    EXPECT_EQ(parsed.frequency_entry_count, 4582U);

    auto crossed = stream;
    crossed.context_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 4;
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(crossed, {}),
              LzssContextualTansStreamHeaderError::contradictory_parameters);
}

TEST(LzssContextualTansFormat,
     KeepsSixtyFourMiBOuterStreamIdentityClosed) {
    const auto stream = stream_config_64m();
    const auto limits = limits_64m();
    EXPECT_EQ(validate_lzss_contextual_tans_stream_header(stream, limits),
              LzssContextualTansStreamHeaderError::
                  unsupported_context_variant);

    std::array<std::byte, lzss_contextual_tans_stream_header_size> output{};
    std::ranges::fill(output, std::byte{0xa5});
    EXPECT_EQ(serialize_lzss_contextual_tans_stream_header(
                  stream, limits, output),
              LzssContextualTansStreamHeaderError::
                  unsupported_context_variant);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));

    auto base = stream_config_16m();
    base.frame_size = UINT32_C(1) << 26;
    ASSERT_EQ(serialize_lzss_contextual_tans_stream_header(
                  base, limits, output),
              LzssContextualTansStreamHeaderError::none);
    output[14] = std::byte{0x06};
    output[64] = std::byte{0x00};
    output[65] = std::byte{0x00};
    output[66] = std::byte{0x00};
    output[67] = std::byte{0x04};
    output[84] = std::byte{0xf6};
    output[85] = std::byte{0x11};
    output[98] = std::byte{0x05};
    auto parsed = stream_config();
    parsed.original_size = 0xa5a5;
    const auto before = parsed;
    std::size_t consumed = 0xa5a5;
    EXPECT_EQ(parse_lzss_contextual_tans_stream_header(
                  output, limits, parsed, consumed),
              LzssContextualTansStreamHeaderError::
                  unsupported_context_variant);
    EXPECT_EQ(parsed.original_size, before.original_size);
    EXPECT_EQ(consumed, 0xa5a5U);
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

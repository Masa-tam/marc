#include "frame/lzss_contextual_rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::array<std::byte, lzss_contextual_rans_stream_header_size>
canonical_stream_vector() {
    std::array<std::byte, lzss_contextual_rans_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52}; bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02}; bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01}; bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02}; bytes[16] = std::byte{0x04};
    bytes[18] = std::byte{0x03}; bytes[20] = std::byte{0x40};
    bytes[28] = std::byte{0x10}; bytes[32] = std::byte{0x10};
    bytes[40] = std::byte{0x01}; bytes[48] = std::byte{0x10};
    bytes[66] = std::byte{0x01}; bytes[68] = std::byte{0x05};
    bytes[72] = std::byte{0x02}; bytes[73] = std::byte{0x01};
    bytes[80] = std::byte{0x0c}; bytes[81] = std::byte{0x01};
    bytes[82] = std::byte{0x1f}; bytes[84] = std::byte{0xa6};
    bytes[85] = std::byte{0x11}; bytes[96] = std::byte{0x01};
    bytes[98] = std::byte{0x01};
    return bytes;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config() {
    LzssContextualRansStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config_1m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 20;
    stream.dictionary.window_size = UINT32_C(1) << 20;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    stream.frequency_entry_count = 4550;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config_4m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 22;
    stream.dictionary.window_size = UINT32_C(1) << 22;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    stream.frequency_entry_count = 4566;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config_16m() {
    auto stream = stream_config();
    stream.frame_size = UINT32_C(1) << 24;
    stream.dictionary.window_size = UINT32_C(1) << 24;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    stream.frequency_entry_count = 4582;
    return stream;
}

[[nodiscard]] LzssContextualRansStreamHeader stream_config_64m() {
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

} // namespace

TEST(LzssContextualRansStreamFormat, ParsesAndSerializesCanonicalHeader) {
    const auto expected = canonical_stream_vector();
    LzssContextualRansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_rans_stream_header(
                  expected, {}, parsed, consumed),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(consumed, expected.size());
    EXPECT_EQ(parsed.frame_size, 64U);
    EXPECT_EQ(parsed.original_size, 1U);
    EXPECT_EQ(parsed.table_log, 12U);
    EXPECT_EQ(parsed.context_count, 31U);
    EXPECT_EQ(parsed.frequency_entry_count, 4518U);

    std::array<std::byte, lzss_contextual_rans_stream_header_size> output{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream_config(), {}, output),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(output, expected);
}

TEST(LzssContextualRansStreamFormat,
     RejectsTruncationAndRetiredVariantWithoutPublishing) {
    const auto canonical = canonical_stream_vector();
    for (std::size_t size = 0; size < canonical.size(); ++size) {
        LzssContextualRansStreamHeader output{};
        output.original_size = 123;
        std::size_t consumed = 7;
        EXPECT_EQ(parse_lzss_contextual_rans_stream_header(
                      std::span<const std::byte>{canonical}.first(size), {},
                      output, consumed),
                  LzssContextualRansStreamHeaderError::truncated_header)
            << "size=" << size;
        EXPECT_EQ(output.original_size, 123U);
        EXPECT_EQ(consumed, 7U);
    }

    auto retired = canonical;
    retired[18] = std::byte{0x02};
    LzssContextualRansStreamHeader output{};
    output.original_size = 123;
    std::size_t consumed = 7;
    EXPECT_EQ(parse_lzss_contextual_rans_stream_header(
                  retired, {}, output, consumed),
              LzssContextualRansStreamHeaderError::unsupported_entropy_variant);
    EXPECT_EQ(output.original_size, 123U);
    EXPECT_EQ(consumed, 7U);
}

TEST(LzssContextualRansStreamFormat,
     SerializesTransactionallyAndEnforcesLimits) {
    auto stream = stream_config();
    stream.context_count = 30;
    std::array<std::byte, lzss_contextual_rans_stream_header_size> output{};
    output.fill(std::byte{0xcc});
    EXPECT_EQ(serialize_lzss_contextual_rans_stream_header(stream, {}, output),
              LzssContextualRansStreamHeaderError::invalid_entropy_parameters);
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{0xcc};
    }));

    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries = 126975;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(
                  stream_config(), limits),
              LzssContextualRansStreamHeaderError::limit_exceeded);
}

TEST(LzssContextualRansStreamFormat,
     RoundTripsSelectedOneMiBIdentityAndRejectsCrossedPairs) {
    const auto stream = stream_config_1m();
    std::array<std::byte, lzss_contextual_rans_stream_header_size> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, {}, encoded),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{0x03});
    EXPECT_EQ(encoded[98], std::byte{0x02});
    EXPECT_EQ(encoded[66], std::byte{0x10});
    EXPECT_EQ(encoded[84], std::byte{0xc6});
    EXPECT_EQ(encoded[85], std::byte{0x11});

    LzssContextualRansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_rans_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 20);
    EXPECT_EQ(parsed.dictionary_variant, 3U);
    EXPECT_EQ(parsed.context_variant, 2U);
    EXPECT_EQ(parsed.frequency_entry_count, 4550U);

    auto crossed = stream;
    crossed.context_variant = 1;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.frequency_entry_count = 4518;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::invalid_entropy_parameters);
}

TEST(LzssContextualRansStreamFormat,
     RoundTripsFourMiBIdentityAndSelectsSevenFBound) {
    const auto stream = stream_config_4m();
    std::array<std::byte, lzss_contextual_rans_stream_header_size> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, {}, encoded),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(encoded[14], std::byte{0x04});
    EXPECT_EQ(encoded[98], std::byte{0x03});
    EXPECT_EQ(encoded[84], std::byte{0xd6});
    EXPECT_EQ(encoded[85], std::byte{0x11});

    LzssContextualRansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_rans_stream_header(
                  encoded, {}, parsed, consumed),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 22);
    EXPECT_EQ(parsed.dictionary_variant, 4U);
    EXPECT_EQ(parsed.context_variant, 3U);
    EXPECT_EQ(parsed.frequency_entry_count, 4566U);

    auto crossed = stream;
    crossed.context_variant = 2;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);

    auto small = stream_config();
    small.frame_size = 5;
    small.original_size = 5;
    LzssContextualRansFrameHeader header{
        0, 0, 5, 1, 5, 32, 8,
        static_cast<std::uint32_t>(
            marc::entropy::internal::contextual_rans_min_descriptor_size),
        0, 0};
    EXPECT_EQ(validate_lzss_contextual_rans_frame_header(
                  header, {small, {}, 0, 0}),
              LzssContextualRansFrameHeaderError::contradictory_counts);
    auto selected = stream;
    selected.frame_size = 5;
    selected.original_size = 5;
    EXPECT_EQ(validate_lzss_contextual_rans_frame_header(
                  header, {selected, {}, 0, 0}),
              LzssContextualRansFrameHeaderError::none);
}

TEST(LzssContextualRansStreamFormat,
     RoundTripsSixteenMiBIdentityAndRejectsCrossedPairs) {
    const auto stream = stream_config_16m();
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(stream, {}),
              LzssContextualRansStreamHeaderError::none);

    std::array<std::byte, lzss_contextual_rans_stream_header_size> output{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, {}, output),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(output[14], std::byte{0x05});
    EXPECT_EQ(output[98], std::byte{0x04});
    EXPECT_EQ(output[84], std::byte{0xe6});
    EXPECT_EQ(output[85], std::byte{0x11});

    LzssContextualRansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_rans_stream_header(
                  output, {}, parsed, consumed),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(consumed, output.size());
    EXPECT_EQ(parsed.dictionary_variant, 5U);
    EXPECT_EQ(parsed.context_variant, 4U);
    EXPECT_EQ(parsed.frequency_entry_count, 4582U);

    auto crossed = stream;
    crossed.context_variant = 3;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 4;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, {}),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
}

TEST(LzssContextualRansStreamFormat,
     SerializesAndParsesPrivateSixtyFourMiBIdentity) {
    const auto limits = limits_64m();
    const auto stream = stream_config_64m();
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(stream, limits),
              LzssContextualRansStreamHeaderError::none);

    auto base = stream_config_16m();
    base.frame_size = stream.frame_size;
    std::array<std::byte, lzss_contextual_rans_stream_header_size> encoded{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  base, limits, encoded),
              LzssContextualRansStreamHeaderError::none);
    encoded[14] = std::byte{0x06};
    encoded[64] = std::byte{0x00};
    encoded[65] = std::byte{0x00};
    encoded[66] = std::byte{0x00};
    encoded[67] = std::byte{0x04};
    encoded[84] = std::byte{0xf6};
    encoded[85] = std::byte{0x11};
    encoded[98] = std::byte{0x05};

    LzssContextualRansStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_lzss_contextual_rans_stream_header(
                  encoded, limits, parsed, consumed),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(consumed, encoded.size());
    EXPECT_EQ(parsed.dictionary.window_size, UINT32_C(1) << 26);
    EXPECT_EQ(parsed.dictionary_variant, 6U);
    EXPECT_EQ(parsed.context_variant, 5U);
    EXPECT_EQ(parsed.frequency_entry_count, 4598U);

    std::array<std::byte, lzss_contextual_rans_stream_header_size> output{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, limits, output),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(output, encoded);

    auto crossed = stream;
    crossed.context_variant = 4;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, limits),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
    crossed = stream;
    crossed.dictionary_variant = 5;
    EXPECT_EQ(validate_lzss_contextual_rans_stream_header(crossed, limits),
              LzssContextualRansStreamHeaderError::contradictory_parameters);
}

TEST(LzssContextualRansFrameFormat,
     SixtyFourMiBIdentitySelectsEightFAndThirtySixTBounds) {
    auto stream = stream_config_64m();
    stream.frame_size = 5;
    stream.original_size = 5;
    const auto limits = limits_64m();
    LzssContextualRansFrameHeader header{
        0, 0, 5, 1, 5, 36, 8,
        static_cast<std::uint32_t>(
            marc::entropy::internal::contextual_rans_min_descriptor_size),
        0, 0};
    EXPECT_EQ(validate_lzss_contextual_rans_frame_header(
                  header, {stream, limits, 0, 0}),
              LzssContextualRansFrameHeaderError::none);

    header.decision_count = 37;
    EXPECT_EQ(validate_lzss_contextual_rans_frame_header(
                  header, {stream, limits, 0, 0}),
              LzssContextualRansFrameHeaderError::contradictory_counts);
    header.decision_count = 36;
    auto crossed = stream;
    crossed.context_variant = 4;
    EXPECT_EQ(validate_lzss_contextual_rans_frame_header(
                  header, {crossed, limits, 0, 0}),
              LzssContextualRansFrameHeaderError::invalid_stream_header);
}

#include "frame/lzss_contextual_rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::array<std::byte, lzss_contextual_rans_stream_header_size>
stream_vector() {
    std::array<std::byte, lzss_contextual_rans_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52}; bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02}; bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01}; bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02}; bytes[16] = std::byte{0x04};
    bytes[18] = std::byte{0x02}; bytes[20] = std::byte{0x40};
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

[[nodiscard]] std::vector<std::byte> frame_vector() {
    std::vector<std::byte> bytes(9124);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01}; bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02}; bytes[32] = std::byte{0x08};
    bytes[36] = std::byte{0x5c}; bytes[37] = std::byte{0x23};
    bytes[64] = std::byte{0x02}; bytes[68] = std::byte{0x08};
    bytes[72] = std::byte{0x0c}; bytes[74] = std::byte{0x1f};
    bytes[76] = std::byte{0xa6}; bytes[77] = std::byte{0x11};
    bytes[80] = std::byte{0x00}; bytes[81] = std::byte{0x10};
    bytes[222] = std::byte{0x00}; bytes[223] = std::byte{0x10};
    bytes[9119] = std::byte{0x80};
    return bytes;
}

[[nodiscard]] LzssContextualRansFrameValidationContext frame_context(
    const LzssContextualRansStreamHeader& stream,
    const marc::core::DecoderLimits& limits) {
    return {stream, limits, 0, 0};
}

} // namespace

TEST(LzssContextualRansStreamFormat, ParsesAndSerializesSpecifiedHeader) {
    const auto expected = stream_vector();
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

TEST(LzssContextualRansStreamFormat, RejectsTruncationAndWrongEntropyIdentity) {
    const auto canonical = stream_vector();
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
    auto wrong = canonical;
    wrong[16] = std::byte{0x03};
    LzssContextualRansStreamHeader output{};
    std::size_t consumed{};
    EXPECT_EQ(parse_lzss_contextual_rans_stream_header(
                  wrong, {}, output, consumed),
              LzssContextualRansStreamHeaderError::unknown_entropy_algorithm);
}

TEST(LzssContextualRansStreamFormat, SerializesTransactionallyAndEnforcesLimits) {
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

TEST(LzssContextualRansFrameFormat, PreflightsSpecifiedOneLiteralFrame) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    LzssContextualRansFrameLayout layout{};
    const auto result = preflight_lzss_contextual_rans_frame(
        frame, frame_context(stream, {}), layout);
    ASSERT_EQ(result.error, LzssContextualRansFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, frame.size());
    EXPECT_EQ(layout.header.token_count, 1U);
    EXPECT_EQ(layout.header.event_count, 2U);
    EXPECT_EQ(layout.header.decision_count, 2U);
    EXPECT_EQ(layout.descriptor.frequencies[0], 4096U);
    EXPECT_EQ(layout.descriptor.frequencies[71], 4096U);

    std::array<std::byte, lzss_contextual_rans_frame_header_size> output{};
    EXPECT_EQ(serialize_lzss_contextual_rans_frame_header(
                  layout.header, frame_context(stream, {}), output),
              LzssContextualRansFrameHeaderError::none);
    EXPECT_TRUE(std::ranges::equal(output,
        std::span<const std::byte>{frame}.first(output.size())));
}

TEST(LzssContextualRansFrameFormat, RejectsAllTruncatedExtentsAtomically) {
    const auto frame = frame_vector();
    const auto stream = stream_config();
    for (const auto size : std::array<std::size_t, 4>{63, 64, 9115, 9123}) {
        LzssContextualRansFrameLayout layout{};
        layout.serialized_size = 17;
        const auto result = preflight_lzss_contextual_rans_frame(
            std::span<const std::byte>{frame}.first(size),
            frame_context(stream, {}), layout);
        EXPECT_NE(result.error, LzssContextualRansFramePreflightError::none)
            << "size=" << size;
        EXPECT_EQ(layout.serialized_size, 17U);
    }
}

TEST(LzssContextualRansFrameFormat, RejectsMalformedDescriptorAndFrameLimit) {
    auto frame = frame_vector();
    const auto stream = stream_config();
    frame[80] = std::byte{0x01};
    LzssContextualRansFrameLayout layout{};
    auto result = preflight_lzss_contextual_rans_frame(
        frame, frame_context(stream, {}), layout);
    EXPECT_EQ(result.error,
              LzssContextualRansFramePreflightError::descriptor_error);

    frame = frame_vector();
    auto limits = marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes = frame.size() - 1;
    result = preflight_lzss_contextual_rans_frame(
        frame, frame_context(stream, limits), layout);
    EXPECT_EQ(result.error,
              LzssContextualRansFramePreflightError::header_error);
    EXPECT_EQ(result.header_error,
              LzssContextualRansFrameHeaderError::invalid_stream_header);
}

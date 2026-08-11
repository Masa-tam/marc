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

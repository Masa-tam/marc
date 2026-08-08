#include "entropy/contextual_rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::ContextualRansFormatError;
using marc::entropy::internal::contextual_rans_descriptor_size;

[[nodiscard]] ContextualRansDescriptor literal_a_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

TEST(ContextualRansFormat, SerializesAndParsesOneLiteralVector) {
    const auto descriptor = literal_a_descriptor();
    std::array<std::byte, contextual_rans_descriptor_size> bytes{};
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, 2, 8, {}, bytes),
              ContextualRansFormatError::none);
    constexpr std::array prefix{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{bytes}.first(prefix.size()), prefix));
    EXPECT_EQ(bytes[16], std::byte{0x00});
    EXPECT_EQ(bytes[17], std::byte{0x10});
    EXPECT_EQ(bytes[158], std::byte{0x00});
    EXPECT_EQ(bytes[159], std::byte{0x10});
    EXPECT_EQ(std::count_if(
                  bytes.begin() + 16, bytes.end(),
                  [](const std::byte value) { return value != std::byte{}; }),
              2);

    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 2, 8, {}, parsed),
              ContextualRansFormatError::none);
    EXPECT_EQ(parsed.decision_count, descriptor.decision_count);
    EXPECT_EQ(parsed.payload_size, descriptor.payload_size);
    EXPECT_EQ(parsed.frequencies, descriptor.frequencies);
}

TEST(ContextualRansFormat, AcceptsZeroUnusedSlicesAndCompleteUsedSlices) {
    auto descriptor = literal_a_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::none);

    descriptor.frequencies[0] = 2048;
    descriptor.frequencies[1] = 2048;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::none);
}

TEST(ContextualRansFormat, RejectsMalformedSlicesWithoutPublishing) {
    auto descriptor = literal_a_descriptor();
    descriptor.frequencies[0] = 4095;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::invalid_frequency_table);

    std::array<std::byte, contextual_rans_descriptor_size> bytes{};
    auto valid = literal_a_descriptor();
    ASSERT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  valid, 2, 8, {}, bytes),
              ContextualRansFormatError::none);
    bytes[16] = std::byte{0xff};
    bytes[17] = std::byte{0x0f};
    ContextualRansDescriptor parsed = valid;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 2, 8, {}, parsed),
              ContextualRansFormatError::invalid_frequency_table);
    EXPECT_EQ(parsed.frequencies, valid.frequencies);
}

TEST(ContextualRansFormat, RejectsFieldsPayloadBoundsAndContradictions) {
    auto descriptor = literal_a_descriptor();
    descriptor.decision_count = 0;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 0, 8, {}),
              ContextualRansFormatError::invalid_decision_count);
    descriptor = literal_a_descriptor();
    descriptor.payload_size = 7;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 7, {}),
              ContextualRansFormatError::invalid_payload_size);
    descriptor.payload_size = 13;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 13, {}),
              ContextualRansFormatError::invalid_payload_size);
    descriptor = literal_a_descriptor();
    descriptor.table_log = 11;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::invalid_table_log);
    descriptor = literal_a_descriptor();
    descriptor.flags = 1;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::unknown_flags);
    descriptor = literal_a_descriptor();
    descriptor.context_count = 30;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::invalid_context_count);
    descriptor = literal_a_descriptor();
    --descriptor.frequency_entry_count;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}),
              ContextualRansFormatError::invalid_frequency_entry_count);
    descriptor = literal_a_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 3, 8, {}),
              ContextualRansFormatError::contradictory_size);
}

TEST(ContextualRansFormat, EnforcesBlockPayloadTableAndBufferLimits) {
    const auto descriptor = literal_a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, limits),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_compressed_payload_size = 7;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, limits),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_rans_decode_table_entries - 1;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, limits),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes = contextual_rans_descriptor_size + 7;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, limits),
              ContextualRansFormatError::limit_exceeded);
}

TEST(ContextualRansFormat, FailedSerializationLeavesOutputUntouched) {
    auto descriptor = literal_a_descriptor();
    descriptor.flags = 1;
    std::array<std::byte, contextual_rans_descriptor_size> output{};
    std::fill(output.begin(), output.end(), std::byte{0xa5});
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, 2, 8, {}, output),
              ContextualRansFormatError::unknown_flags);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte value) { return value == std::byte{0xa5}; }));
}

} // namespace

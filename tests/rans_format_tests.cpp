#include "entropy/rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

using marc::entropy::internal::RansDescriptor;
using marc::entropy::internal::RansFormatError;

[[nodiscard]] RansDescriptor a_descriptor() {
    RansDescriptor descriptor{};
    descriptor.symbol_count = 1;
    descriptor.payload_size = 8;
    descriptor.frequencies[0x41] = 4096;
    return descriptor;
}

TEST(RansFormat, SerializesAndParsesSparseHandVector) {
    const auto descriptor = a_descriptor();
    std::array<std::byte, marc::entropy::internal::rans_descriptor_size> bytes{};
    ASSERT_EQ(marc::entropy::internal::serialize_rans_descriptor(
                  descriptor, 1, 8, {}, bytes),
              RansFormatError::none);
    constexpr std::array prefix{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{12}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{bytes}.first(16), prefix));
    EXPECT_EQ(bytes[146], std::byte{0x00});
    EXPECT_EQ(bytes[147], std::byte{0x10});

    RansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_rans_descriptor(
                  bytes, 1, 8, {}, parsed),
              RansFormatError::none);
    EXPECT_EQ(parsed.frequencies, descriptor.frequencies);
}

TEST(RansFormat, RejectsMalformedTableWithoutPublishing) {
    auto descriptor = a_descriptor();
    descriptor.frequencies[0x41] = 4095;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, {}),
              RansFormatError::invalid_frequency_table);

    std::array<std::byte, marc::entropy::internal::rans_descriptor_size> bytes{};
    bytes[0] = std::byte{1};
    bytes[4] = std::byte{8};
    bytes[8] = std::byte{12};
    bytes[146] = std::byte{0xff};
    bytes[147] = std::byte{0x0f};
    RansDescriptor parsed = a_descriptor();
    EXPECT_EQ(marc::entropy::internal::parse_rans_descriptor(
                  bytes, 1, 8, {}, parsed),
              RansFormatError::invalid_frequency_table);
    EXPECT_EQ(parsed.frequencies[0x41], 4096U);
}

TEST(RansFormat, RejectsFieldsReservedBytesAndContradictions) {
    auto descriptor = a_descriptor();
    descriptor.table_log = 11;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, {}),
              RansFormatError::invalid_table_log);
    descriptor = a_descriptor();
    descriptor.flags = 1;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, {}),
              RansFormatError::unknown_flags);
    descriptor = a_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 2, 8, {}),
              RansFormatError::contradictory_size);

    std::array<std::byte, marc::entropy::internal::rans_descriptor_size> bytes{};
    bytes[0] = std::byte{1};
    bytes[4] = std::byte{8};
    bytes[8] = std::byte{12};
    bytes[15] = std::byte{1};
    RansDescriptor parsed{};
    EXPECT_EQ(marc::entropy::internal::parse_rans_descriptor(
                  bytes, 1, 8, {}, parsed),
              RansFormatError::nonzero_reserved);
}

TEST(RansFormat, EnforcesPayloadBlockTableAndBufferLimits) {
    const auto descriptor = a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 0;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, limits),
              RansFormatError::limit_exceeded);
    limits = {};
    limits.max_entropy_table_entries = 4095;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, limits),
              RansFormatError::limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes =
        marc::entropy::internal::rans_descriptor_size + 7;
    EXPECT_EQ(marc::entropy::internal::validate_rans_descriptor(
                  descriptor, 1, 8, limits),
              RansFormatError::limit_exceeded);
}

} // namespace

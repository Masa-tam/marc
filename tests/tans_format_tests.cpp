#include "entropy/tans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <span>

namespace {
using marc::entropy::internal::TansDescriptor;
using marc::entropy::internal::TansFormatError;

[[nodiscard]] TansDescriptor a_descriptor() {
    TansDescriptor descriptor{};
    descriptor.symbol_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequencies[0x41] = 4096;
    return descriptor;
}

TEST(TansFormat, SerializesAndParsesHandVector) {
    const auto descriptor = a_descriptor();
    std::array<std::byte, marc::entropy::internal::tans_descriptor_size> bytes{};
    ASSERT_EQ(marc::entropy::internal::serialize_tans_descriptor(
                  descriptor, 1, 2, {}, bytes), TansFormatError::none);
    constexpr std::array prefix{
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{12}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{bytes}.first(16), prefix));
    EXPECT_EQ(bytes[146], std::byte{0x00});
    EXPECT_EQ(bytes[147], std::byte{0x10});
    TansDescriptor parsed{};
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor(
                  bytes, 1, 2, {}, parsed), TansFormatError::none);
    EXPECT_EQ(parsed.frequencies, descriptor.frequencies);
}

TEST(TansFormat, ValidatesBitExtentAndFields) {
    auto descriptor = a_descriptor();
    descriptor.final_valid_bits = 1;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 2, {}),
              TansFormatError::invalid_valid_bits);
    descriptor = a_descriptor();
    descriptor.payload_size = 3;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 3, {}),
              TansFormatError::invalid_valid_bits);
    descriptor.final_valid_bits = 8;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 3, {}), TansFormatError::none);
    descriptor.table_log = 11;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 3, {}),
              TansFormatError::invalid_table_log);
    descriptor = a_descriptor();
    descriptor.flags = 1;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 2, {}), TansFormatError::unknown_flags);
}

TEST(TansFormat, RejectsMalformedInputWithoutPublishing) {
    std::array<std::byte, marc::entropy::internal::tans_descriptor_size> bytes{};
    bytes[0] = std::byte{1};
    bytes[4] = std::byte{2};
    bytes[8] = std::byte{12};
    bytes[146] = std::byte{0xff};
    bytes[147] = std::byte{0x0f};
    TansDescriptor parsed = a_descriptor();
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor(
                  bytes, 1, 2, {}, parsed),
              TansFormatError::invalid_frequency_table);
    EXPECT_EQ(parsed.frequencies[0x41], 4096U);
    bytes[15] = std::byte{1};
    EXPECT_EQ(marc::entropy::internal::parse_tans_descriptor(
                  bytes, 1, 2, {}, parsed),
              TansFormatError::nonzero_reserved);
}

TEST(TansFormat, EnforcesSizesAndLocalLimits) {
    const auto descriptor = a_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 2, 2, {}),
              TansFormatError::contradictory_size);
    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = 4095;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 2, limits),
              TansFormatError::limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes =
        marc::entropy::internal::tans_descriptor_size + 1;
    EXPECT_EQ(marc::entropy::internal::validate_tans_descriptor(
                  descriptor, 1, 2, limits),
              TansFormatError::limit_exceeded);
}
} // namespace

#include "core/endian.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

TEST(LittleEndianTest, StoresHandCheckableValues) {
    std::array<std::byte, 14> serialized{};
    ASSERT_TRUE(marc::core::store_le<std::uint16_t>(serialized, 0, 0x1234));
    ASSERT_TRUE(marc::core::store_le<std::uint32_t>(
        serialized, 2, UINT32_C(0x12345678)));
    ASSERT_TRUE(marc::core::store_le<std::uint64_t>(
        serialized, 6, UINT64_C(0x0123456789abcdef)));

    const std::array expected{
        std::byte{0x34}, std::byte{0x12}, std::byte{0x78}, std::byte{0x56},
        std::byte{0x34}, std::byte{0x12}, std::byte{0xef}, std::byte{0xcd},
        std::byte{0xab}, std::byte{0x89}, std::byte{0x67}, std::byte{0x45},
        std::byte{0x23}, std::byte{0x01}};
    EXPECT_EQ(serialized, expected);
}

TEST(LittleEndianTest, LoadsHandCheckableValue) {
    const std::array input{
        std::byte{0xef}, std::byte{0xcd}, std::byte{0xab}, std::byte{0x89},
        std::byte{0x67}, std::byte{0x45}, std::byte{0x23}, std::byte{0x01}};
    std::uint64_t loaded{};
    ASSERT_TRUE(marc::core::load_le<std::uint64_t>(input, 0, loaded));
    EXPECT_EQ(loaded, UINT64_C(0x0123456789abcdef));
}

TEST(LittleEndianTest, RejectsOutOfBoundsStoreAndLoad) {
    std::array<std::byte, 14> buffer{};
    EXPECT_FALSE(marc::core::store_le<std::uint32_t>(buffer, 12, 0));
    std::uint64_t loaded{};
    EXPECT_FALSE(marc::core::load_le<std::uint64_t>(buffer, 7, loaded));
}

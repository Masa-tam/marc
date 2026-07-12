#include "core/bit_io.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

using marc::core::BitIoStatus;

TEST(BitWriterTest, DefersFullByteUntilOutputIsAvailable) {
    marc::core::BitWriter writer;
    auto result = writer.write_bits(0x4d, 8, {});
    EXPECT_EQ(result.bits_consumed, 8);
    EXPECT_EQ(result.bytes_produced, 0U);

    std::array<std::byte, 1> output{};
    result = writer.write_bits(0, 1, output);
    EXPECT_EQ(result.bits_consumed, 1);
    EXPECT_EQ(result.bytes_produced, 1U);
    EXPECT_EQ(output[0], std::byte{0x4d});
}

TEST(BitWriterTest, RequiresOutputToFinishPendingBits) {
    marc::core::BitWriter writer;
    ASSERT_EQ(writer.write_bits(1, 1, {}).bits_consumed, 1);
    EXPECT_EQ(writer.finish({}).status, BitIoStatus::need_output);

    std::array<std::byte, 1> output{};
    const auto result = writer.finish(output);
    EXPECT_EQ(result.status, BitIoStatus::finished);
    EXPECT_EQ(result.bytes_produced, 1U);
    EXPECT_EQ(output[0], std::byte{0x01});
}

TEST(BitWriterTest, ZeroPadsFinalPartialByte) {
    marc::core::BitWriter writer;
    ASSERT_EQ(writer.write_bits(5, 3, {}).bits_consumed, 3);
    std::array<std::byte, 1> output{};
    ASSERT_EQ(writer.finish(output).status, BitIoStatus::finished);
    EXPECT_EQ(output[0], std::byte{0x05});
}

TEST(BitReaderTest, ReadsBufferedBitsWithoutNewInput) {
    marc::core::BitReader reader;
    EXPECT_EQ(reader.read_bits({}, 3).status, BitIoStatus::need_input);

    const std::array input{std::byte{0x4d}};
    const auto low = reader.read_bits(input, 3);
    EXPECT_EQ(low.status, BitIoStatus::complete);
    EXPECT_EQ(low.value, 5U);
    EXPECT_EQ(low.bytes_consumed, 1U);

    const auto high = reader.read_bits({}, 5);
    EXPECT_EQ(high.status, BitIoStatus::complete);
    EXPECT_EQ(high.value, 9U);
    EXPECT_EQ(high.bytes_consumed, 0U);
}

TEST(BitReaderTest, RejectsNonZeroPaddingInStrictMode) {
    marc::core::BitReader reader;
    const std::array input{std::byte{0xfd}};
    ASSERT_EQ(reader.read_bits(input, 3).value, 5U);
    EXPECT_EQ(reader.align_to_byte(true), BitIoStatus::invalid_padding);
}

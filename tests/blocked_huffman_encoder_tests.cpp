#include "entropy/blocked_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::entropy::internal::BlockedHuffmanDescriptor;
using marc::entropy::internal::BlockedHuffmanEncodeError;

} // namespace

TEST(BlockedHuffmanEncoderTest, RejectsEmptyBlockWithoutMutation) {
    std::array<std::byte, 256> model{};
    std::array<std::byte, 1> payload{std::byte{0x5a}};
    BlockedHuffmanDescriptor descriptor{7, 7, 0, 1, 8};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            {}, marc::core::DecoderLimits{}, model, payload, descriptor);
    EXPECT_EQ(result.error, BlockedHuffmanEncodeError::empty_input);
    EXPECT_EQ(payload[0], std::byte{0x5a});
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(BlockedHuffmanEncoderTest, EmitsHandCheckableFourByteRawBlock) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x41},
        std::byte{0x41}, std::byte{0x41}};
    std::array<std::byte, 256> model{};
    std::array<std::byte, 4> payload{};
    BlockedHuffmanDescriptor descriptor{};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    ASSERT_EQ(result.error, BlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.model_size, 0U);
    EXPECT_EQ(result.payload_size, input.size());
    EXPECT_EQ(payload, input);
    EXPECT_EQ(descriptor.flags,
              marc::entropy::internal::blocked_huffman_raw_flag);
    EXPECT_EQ(descriptor.final_valid_bits, 8);
}

TEST(BlockedHuffmanEncoderTest, EncodesOneSymbolHuffmanPayload) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    std::array<std::byte, 256> model{};
    std::array<std::byte, 38> payload{};
    BlockedHuffmanDescriptor descriptor{};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    ASSERT_EQ(result.error, BlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.model_size, 256U);
    EXPECT_EQ(result.payload_size, 38U);
    EXPECT_EQ(model[0x41], std::byte{1});
    EXPECT_EQ(std::ranges::count(model, std::byte{0}), 255);
    EXPECT_TRUE(std::ranges::all_of(
        payload, [](const std::byte value) { return value == std::byte{0}; }));
    EXPECT_EQ(descriptor.flags, 0);
    EXPECT_EQ(descriptor.final_valid_bits, 4);
}

TEST(BlockedHuffmanEncoderTest, PacksTwoSymbolsLsbFirst) {
    std::vector<std::byte> input;
    input.reserve(512);
    for (std::size_t index = 0; index < 256; ++index) {
        input.push_back(std::byte{0x41});
        input.push_back(std::byte{0x42});
    }
    std::array<std::byte, 256> model{};
    std::array<std::byte, 64> payload{};
    BlockedHuffmanDescriptor descriptor{};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    ASSERT_EQ(result.error, BlockedHuffmanEncodeError::none);
    EXPECT_EQ(model[0x41], std::byte{1});
    EXPECT_EQ(model[0x42], std::byte{1});
    EXPECT_TRUE(std::ranges::all_of(
        payload,
        [](const std::byte value) { return value == std::byte{0xaa}; }));
    EXPECT_EQ(descriptor.final_valid_bits, 8);
}

TEST(BlockedHuffmanEncoderTest, SelectsRawWhenStoredSizesTie) {
    std::vector<std::byte> input;
    input.reserve(293);
    for (std::size_t index = 0; index < 293; ++index) {
        input.push_back(index % 2 == 0 ? std::byte{0x41}
                                      : std::byte{0x42});
    }
    std::array<std::byte, 256> model{};
    std::vector<std::byte> payload(input.size());
    BlockedHuffmanDescriptor descriptor{};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    ASSERT_EQ(result.error, BlockedHuffmanEncodeError::none);
    EXPECT_EQ(result.model_size, 0U);
    EXPECT_EQ(descriptor.flags,
              marc::entropy::internal::blocked_huffman_raw_flag);
    EXPECT_EQ(payload, input);
}

TEST(BlockedHuffmanEncoderTest, ReportsCapacityBeforeWritingOutput) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    std::array<std::byte, 255> model{};
    std::array<std::byte, 38> payload{};
    model.fill(std::byte{0x5a});
    payload.fill(std::byte{0x6b});
    BlockedHuffmanDescriptor descriptor{7, 7, 0, 1, 8};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    EXPECT_EQ(result.error,
              BlockedHuffmanEncodeError::model_output_too_small);
    EXPECT_EQ(result.model_size, 256U);
    EXPECT_EQ(result.payload_size, 38U);
    EXPECT_TRUE(std::ranges::all_of(
        model,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_TRUE(std::ranges::all_of(
        payload,
        [](const std::byte value) { return value == std::byte{0x6b}; }));
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

TEST(BlockedHuffmanEncoderTest, ChecksPayloadBeforeWritingModel) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    std::array<std::byte, 256> model{};
    std::array<std::byte, 37> payload{};
    model.fill(std::byte{0x5a});
    BlockedHuffmanDescriptor descriptor{7, 7, 0, 1, 8};
    const auto result =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    EXPECT_EQ(result.error,
              BlockedHuffmanEncodeError::payload_output_too_small);
    EXPECT_EQ(result.model_size, 256U);
    EXPECT_EQ(result.payload_size, 38U);
    EXPECT_TRUE(std::ranges::all_of(
        model,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(descriptor.symbol_count, 7U);
}

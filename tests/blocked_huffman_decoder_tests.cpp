#include "entropy/blocked_huffman_decoder.hpp"
#include "entropy/blocked_huffman_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

using marc::entropy::internal::BlockedHuffmanDecodeError;
using marc::entropy::internal::BlockedHuffmanDescriptor;
using marc::entropy::internal::BlockedHuffmanEncodeError;

void expect_round_trip(const std::vector<std::byte>& input) {
    std::array<std::byte, 256> model{};
    std::vector<std::byte> payload(input.size());
    BlockedHuffmanDescriptor descriptor{};
    const auto encoded =
        marc::entropy::internal::encode_blocked_huffman_block(
            input, marc::core::DecoderLimits{}, model, payload, descriptor);
    ASSERT_EQ(encoded.error, BlockedHuffmanEncodeError::none);
    payload.resize(encoded.payload_size);

    std::vector<std::byte> output(input.size(), std::byte{0x5a});
    const auto decoded =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor,
            std::span<const std::byte>{model}.first(encoded.model_size),
            payload, marc::core::DecoderLimits{}, output);
    ASSERT_EQ(decoded.error, BlockedHuffmanDecodeError::none);
    EXPECT_EQ(decoded.output_size, input.size());
    EXPECT_EQ(output, input);
}

} // namespace

TEST(BlockedHuffmanDecoderTest, RoundTripsRawBlock) {
    expect_round_trip({std::byte{0x41}, std::byte{0x41},
                       std::byte{0x41}, std::byte{0x41}});
}

TEST(BlockedHuffmanDecoderTest, RoundTripsEveryOneByteValue) {
    for (std::size_t value = 0; value < 256; ++value) {
        expect_round_trip(
            {static_cast<std::byte>(static_cast<unsigned int>(value))});
    }
}

TEST(BlockedHuffmanDecoderTest, RoundTripsOneSymbolHuffmanBlock) {
    expect_round_trip(std::vector<std::byte>(300, std::byte{0x41}));
}

TEST(BlockedHuffmanDecoderTest, RoundTripsTwoSymbolHuffmanBlock) {
    std::vector<std::byte> input;
    for (std::size_t index = 0; index < 512; ++index) {
        input.push_back(index % 2 == 0 ? std::byte{0x41}
                                      : std::byte{0x42});
    }
    expect_round_trip(input);
}

TEST(BlockedHuffmanDecoderTest, ReportsExactRequiredOutputCapacity) {
    const auto descriptor = BlockedHuffmanDescriptor{4, 4, 0, 1, 8};
    const std::array payload{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    std::array<std::byte, 3> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, {}, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::output_too_small);
    EXPECT_EQ(result.output_size, 4U);
}

TEST(BlockedHuffmanDecoderTest, RejectsInvalidModelWithoutOutput) {
    const auto descriptor = BlockedHuffmanDescriptor{1024, 128, 256, 0, 8};
    std::array<std::byte, 256> model{};
    model[0] = std::byte{1};
    model[1] = std::byte{1};
    model[2] = std::byte{1};
    std::array<std::byte, 128> payload{};
    std::array<std::byte, 1024> output{};
    output.fill(std::byte{0x5a});
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, model, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::invalid_model);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanDecoderTest, RejectsInvalidSingleSymbolPath) {
    const auto descriptor = BlockedHuffmanDescriptor{300, 38, 256, 0, 4};
    std::array<std::byte, 256> model{};
    model[0x41] = std::byte{1};
    std::array<std::byte, 38> payload{};
    payload[0] = std::byte{1};
    std::array<std::byte, 300> output{};
    output.fill(std::byte{0x5a});
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, model, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::invalid_code);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanDecoderTest, DetectsSemanticPayloadTruncation) {
    const auto descriptor = BlockedHuffmanDescriptor{1024, 128, 256, 0, 8};
    std::array<std::byte, 256> model{};
    model[0] = std::byte{1};
    model[1] = std::byte{2};
    model[2] = std::byte{2};
    std::array<std::byte, 128> payload{};
    payload.fill(std::byte{0xff});
    std::array<std::byte, 1024> output{};
    output.fill(std::byte{0x5a});
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, model, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::truncated_payload);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(BlockedHuffmanDecoderTest, RejectsTrailingBits) {
    const auto descriptor = BlockedHuffmanDescriptor{1024, 129, 256, 0, 1};
    std::array<std::byte, 256> model{};
    model[0x41] = std::byte{1};
    std::array<std::byte, 129> payload{};
    std::array<std::byte, 1024> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, model, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::trailing_bits);
}

TEST(BlockedHuffmanDecoderTest, RejectsNonzeroHighPadding) {
    const auto descriptor = BlockedHuffmanDescriptor{1025, 129, 256, 0, 1};
    std::array<std::byte, 256> model{};
    model[0x41] = std::byte{1};
    std::array<std::byte, 129> payload{};
    payload.back() = std::byte{0x80};
    std::array<std::byte, 1025> output{};
    const auto result =
        marc::entropy::internal::decode_blocked_huffman_block(
            descriptor, model, payload, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, BlockedHuffmanDecodeError::nonzero_padding);
}

TEST(BlockedHuffmanDecoderTest, RequiresExactSerializedRegionSizes) {
    const auto descriptor = BlockedHuffmanDescriptor{300, 38, 256, 0, 4};
    std::array<std::byte, 255> short_model{};
    std::array<std::byte, 38> payload{};
    std::array<std::byte, 300> output{};
    EXPECT_EQ(marc::entropy::internal::decode_blocked_huffman_block(
                  descriptor, short_model, payload,
                  marc::core::DecoderLimits{}, output).error,
              BlockedHuffmanDecodeError::model_size_mismatch);
    EXPECT_EQ(marc::entropy::internal::decode_blocked_huffman_block(
                  descriptor, std::array<std::byte, 256>{},
                  std::span<const std::byte>{payload}.first(37),
                  marc::core::DecoderLimits{}, output).error,
              BlockedHuffmanDecodeError::payload_size_mismatch);
}

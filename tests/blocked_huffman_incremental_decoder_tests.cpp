#include "frame/blocked_huffman_incremental_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::size_t size,
    const std::uint32_t frame_size = 300) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = frame_size;
    stream.entropy_block_size = frame_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> encode(
    const std::vector<std::byte>& input,
    const std::uint32_t frame_size = 300) {
    const auto stream = stream_for(input.size(), frame_size);
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_blocked_huffman_stream(
                  stream, marc::core::DecoderLimits{}, input, encoded).error,
              marc::frame::BlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(BlockedHuffmanIncrementalDecoderTest, AcceptsEveryInputSplit) {
    const std::vector<std::byte> expected{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto encoded = encode(expected, 4);
    for (std::size_t split = 0; split <= encoded.size(); ++split) {
        std::vector<std::byte> encoded_storage(encoded.size());
        std::vector<std::byte> decoded_storage(expected.size());
        std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
        marc::frame::BlockedHuffmanIncrementalDecoder decoder{
            marc::core::DecoderLimits{}, encoded_storage,
            decoded_storage, views};
        if (split != 0) {
            const auto first = decoder.process(
                std::span<const std::byte>{encoded}.first(split), {}, 0);
            ASSERT_TRUE(marc::core::is_valid(first, split, 0));
            ASSERT_EQ(first.status, marc::core::StreamStatus::progress);
        }
        std::vector<std::byte> output(expected.size());
        const auto final = decoder.process(
            std::span<const std::byte>{encoded}.subspan(split), output,
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        ASSERT_TRUE(marc::core::is_valid(
            final, encoded.size() - split, output.size()));
        ASSERT_EQ(final.status, marc::core::StreamStatus::end_of_stream)
            << "split=" << split;
        output.resize(final.output_produced);
        EXPECT_EQ(output, expected) << "split=" << split;
    }
}

TEST(BlockedHuffmanIncrementalDecoderTest, DrainsOneByteAtATime) {
    const std::vector<std::byte> expected(300, std::byte{0x41});
    const auto encoded = encode(expected);
    std::vector<std::byte> encoded_storage(encoded.size());
    std::vector<std::byte> decoded_storage(expected.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage, decoded_storage, views};
    std::vector<std::byte> actual;
    std::array<std::byte, 1> byte{};
    auto result = decoder.process(
        encoded, byte,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    actual.push_back(byte[0]);
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        ASSERT_EQ(result.status, marc::core::StreamStatus::need_output);
        result = decoder.process({}, byte, 0);
        if (result.output_produced != 0) {
            actual.push_back(byte[0]);
        }
    }
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(decoder.process({}, byte, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(BlockedHuffmanIncrementalDecoderTest, AcceptsZeroByteFinalInput) {
    const std::vector<std::byte> expected(300, std::byte{0x41});
    const auto encoded = encode(expected);
    std::vector<std::byte> encoded_storage(encoded.size());
    std::vector<std::byte> decoded_storage(expected.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage, decoded_storage, views};
    EXPECT_EQ(decoder.process(encoded, {}, 0).status,
              marc::core::StreamStatus::progress);
    std::vector<std::byte> output(expected.size());
    const auto result = decoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    output.resize(result.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanIncrementalDecoderTest, MalformedInputIsTerminalAndAtomic) {
    const std::vector<std::byte> expected(300, std::byte{0x41});
    auto encoded = encode(expected);
    encoded.back() = std::byte{1};
    std::vector<std::byte> encoded_storage(encoded.size());
    std::vector<std::byte> decoded_storage(expected.size(), std::byte{0x5a});
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage, decoded_storage, views};
    std::vector<std::byte> output(expected.size(), std::byte{0x6b});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_TRUE(std::ranges::all_of(
        output,
        [](const std::byte value) { return value == std::byte{0x6b}; }));
    EXPECT_TRUE(std::ranges::all_of(
        decoded_storage,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::error);
}

TEST(BlockedHuffmanIncrementalDecoderTest, EndInputDetectsTruncation) {
    const std::vector<std::byte> expected{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto encoded = encode(expected, 4);
    std::vector<std::byte> encoded_storage(encoded.size());
    std::vector<std::byte> decoded_storage(expected.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage, decoded_storage, views};
    const auto result = decoder.process(
        std::span<const std::byte>{encoded}.first(encoded.size() - 1), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

TEST(BlockedHuffmanIncrementalDecoderTest, ReportsWorkStorageLimits) {
    const std::vector<std::byte> expected(300, std::byte{0x41});
    const auto encoded = encode(expected);
    std::vector<std::byte> short_encoded_storage(encoded.size() - 1);
    std::vector<std::byte> decoded_storage(expected.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 1> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder encoded_short{
        marc::core::DecoderLimits{}, short_encoded_storage,
        decoded_storage, views};
    EXPECT_EQ(encoded_short.process(encoded, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    std::vector<std::byte> encoded_storage(encoded.size());
    decoded_storage.resize(expected.size() - 1);
    marc::frame::BlockedHuffmanIncrementalDecoder decoded_short{
        marc::core::DecoderLimits{}, encoded_storage,
        decoded_storage, views};
    EXPECT_EQ(decoded_short.process(
                  encoded, {},
                  marc::core::flag_value(
                      marc::core::ProcessFlags::end_input)).error.code,
              marc::core::ErrorCode::out_of_memory);
}

TEST(BlockedHuffmanIncrementalDecoderTest, ReportsInsufficientFrameViews) {
    std::vector<std::byte> expected(300, std::byte{0x41});
    expected.push_back(std::byte{1});
    const auto encoded = encode(expected, 300);
    std::vector<std::byte> encoded_storage(encoded.size());
    std::vector<std::byte> decoded_storage(expected.size());
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 0> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage, decoded_storage, views};
    const auto result = decoder.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(BlockedHuffmanIncrementalDecoderTest, DecodesEmptyHeaderOnlyStream) {
    const std::vector<std::byte> expected;
    const auto encoded = encode(expected);
    std::vector<std::byte> encoded_storage(encoded.size());
    std::array<std::byte, 1> unused_decoded{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 0> views{};
    marc::frame::BlockedHuffmanIncrementalDecoder decoder{
        marc::core::DecoderLimits{}, encoded_storage,
        std::span<std::byte>{unused_decoded}.first(0), views};
    const auto result = decoder.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 0U);
}

#include "frame/blocked_huffman_incremental_encoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace {

[[nodiscard]] marc::frame::StreamHeader stream_for(
    const std::size_t size) {
    marc::frame::StreamHeader stream{};
    stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 300;
    stream.entropy_block_size = 300;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference_encode(
    const std::vector<std::byte>& input) {
    const auto stream = stream_for(input.size());
    const auto plan = marc::frame::plan_blocked_huffman_stream(
        stream, marc::core::DecoderLimits{}, input);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_blocked_huffman_stream(
                  stream, marc::core::DecoderLimits{}, input, encoded).error,
              marc::frame::BlockedHuffmanStreamCodecError::none);
    return encoded;
}

} // namespace

TEST(BlockedHuffmanIncrementalEncoderTest, MatchesReferenceForEveryInputSplit) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    const auto expected = reference_encode(input);
    for (std::size_t split = 0; split <= input.size(); ++split) {
        std::vector<std::byte> input_storage(input.size());
        std::vector<std::byte> encoded_storage(expected.size());
        marc::frame::BlockedHuffmanIncrementalEncoder encoder{
            stream_for(input.size()), marc::core::DecoderLimits{},
            input_storage, encoded_storage};
        if (split != 0) {
            const auto first = encoder.process(
                std::span<const std::byte>{input}.first(split), {}, 0);
            ASSERT_TRUE(marc::core::is_valid(first, split, 0));
            ASSERT_EQ(first.status, marc::core::StreamStatus::progress);
        }
        std::vector<std::byte> output(expected.size());
        const auto final = encoder.process(
            std::span<const std::byte>{input}.subspan(split), output,
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        ASSERT_TRUE(marc::core::is_valid(
            final, input.size() - split, output.size()));
        ASSERT_EQ(final.status, marc::core::StreamStatus::end_of_stream)
            << "split=" << split;
        output.resize(final.output_produced);
        EXPECT_EQ(output, expected) << "split=" << split;
    }
}

TEST(BlockedHuffmanIncrementalEncoderTest, DrainsOneByteAtATime) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    const auto expected = reference_encode(input);
    std::vector<std::byte> input_storage(input.size());
    std::vector<std::byte> encoded_storage(expected.size());
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    std::vector<std::byte> actual;
    std::array<std::byte, 1> byte{};
    auto result = encoder.process(
        input, byte,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_TRUE(marc::core::is_valid(result, input.size(), byte.size()));
    actual.push_back(byte[0]);
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
        result = encoder.process({}, byte, 0);
        ASSERT_TRUE(marc::core::is_valid(result, 0, byte.size()));
        if (result.output_produced != 0) {
            actual.push_back(byte[0]);
        }
    }
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, byte, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(BlockedHuffmanIncrementalEncoderTest, AcceptsEndInputWithZeroFinalBytes) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    const auto expected = reference_encode(input);
    std::vector<std::byte> input_storage(input.size());
    std::vector<std::byte> encoded_storage(expected.size());
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    EXPECT_EQ(encoder.process(input, {}, 0).status,
              marc::core::StreamStatus::progress);
    std::vector<std::byte> output(expected.size());
    const auto result = encoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    output.resize(result.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanIncrementalEncoderTest, RequestsInputWithoutProgress) {
    std::array<std::byte, 4> input_storage{};
    std::array<std::byte, 256> encoded_storage{};
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(4), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto result = encoder.process({}, {}, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_input);
    EXPECT_TRUE(marc::core::is_valid(result, 0, 0));
}

TEST(BlockedHuffmanIncrementalEncoderTest, RejectsPrematureAndExcessInput) {
    std::array<std::byte, 4> input_storage{};
    std::array<std::byte, 256> encoded_storage{};
    const std::array short_input{std::byte{1}, std::byte{2}};
    marc::frame::BlockedHuffmanIncrementalEncoder premature{
        stream_for(4), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto first = premature.process(
        short_input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(first.status, marc::core::StreamStatus::error);
    EXPECT_EQ(first.input_consumed, short_input.size());

    const std::array long_input{
        std::byte{1}, std::byte{2}, std::byte{3},
        std::byte{4}, std::byte{5}};
    marc::frame::BlockedHuffmanIncrementalEncoder excess{
        stream_for(4), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto second = excess.process(long_input, {}, 0);
    EXPECT_EQ(second.status, marc::core::StreamStatus::error);
    EXPECT_EQ(second.input_consumed, 0U);
}

TEST(BlockedHuffmanIncrementalEncoderTest, RejectsUnsupportedResetBlock) {
    std::array<std::byte, 1> input_storage{};
    std::array<std::byte, 256> encoded_storage{};
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(1), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto result = encoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

TEST(BlockedHuffmanIncrementalEncoderTest, ReportsInsufficientWorkStorage) {
    const std::array input{std::byte{1}};
    std::array<std::byte, 1> input_storage{};
    std::array<std::byte, 1> encoded_storage{};
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(1), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto result = encoder.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(BlockedHuffmanIncrementalEncoderTest, FlushDoesNotAlterLogicalStream) {
    const std::vector<std::byte> input(300, std::byte{0x41});
    const auto expected = reference_encode(input);
    std::vector<std::byte> input_storage(input.size());
    std::vector<std::byte> encoded_storage(expected.size());
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(input.size()), marc::core::DecoderLimits{},
        input_storage, encoded_storage};
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(150), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);
    std::vector<std::byte> output(expected.size());
    const auto final = encoder.process(
        std::span<const std::byte>{input}.subspan(150), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(final.status, marc::core::StreamStatus::end_of_stream);
    output.resize(final.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(BlockedHuffmanIncrementalEncoderTest, EncodesEmptyHeaderOnlyStream) {
    const std::vector<std::byte> input;
    const auto expected = reference_encode(input);
    std::array<std::byte, 1> unused_input_storage{};
    std::vector<std::byte> encoded_storage(expected.size());
    marc::frame::BlockedHuffmanIncrementalEncoder encoder{
        stream_for(0), marc::core::DecoderLimits{},
        std::span<std::byte>{unused_input_storage}.first(0), encoded_storage};
    std::vector<std::byte> output(expected.size());
    const auto result = encoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    output.resize(result.output_produced);
    EXPECT_EQ(output, expected);
}

#include "frame/lz77_blocked_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
    std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 16;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    std::array<std::byte, 32> staging{};
    const auto plan = plan_lz77_blocked_huffman_stream(
        stream, {}, {}, input, staging);
    EXPECT_EQ(plan.error, Lz77BlockedHuffmanStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz77_blocked_huffman_stream(
                  stream, {}, {}, input, staging, output)
                  .error,
              Lz77BlockedHuffmanStreamCodecError::none);
    return output;
}

} // namespace

TEST(Lz77BlockedHuffmanFrameStreamingEncoder, MatchesOracleWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 32> dictionary_staging{};
    std::array<std::byte, 120> frame_encoded{};
    Lz77BlockedHuffmanFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(Lz77BlockedHuffmanFrameStreamingEncoder, EmitsFullFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 32> dictionary_staging{};
    std::array<std::byte, 120> frame_encoded{};
    Lz77BlockedHuffmanFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first<1>(), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lz77_blocked_huffman_stream_prefix_size);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);

    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(1),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(Lz77BlockedHuffmanFrameStreamingEncoder,
     ReportsIndependentWorkspacesAndAggregate) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 32> dictionary_staging{};
    std::array<std::byte, 120> frame_encoded{};
    std::array<std::byte, 500> output{};

    Lz77BlockedHuffmanFrameStreamingEncoder short_input{
        config(input.size()), {}, {},
        std::span<std::byte>{frame_input}.first<1>(), dictionary_staging,
        frame_encoded};
    EXPECT_EQ(short_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);

    Lz77BlockedHuffmanFrameStreamingEncoder short_dictionary{
        config(input.size()), {}, {}, frame_input,
        std::span<std::byte>{dictionary_staging}.first<31>(), frame_encoded};
    EXPECT_EQ(short_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);

    Lz77BlockedHuffmanFrameStreamingEncoder short_encoded{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        std::span<std::byte>{frame_encoded}.first<119>()};
    auto result = short_encoded.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 153;
    Lz77BlockedHuffmanFrameStreamingEncoder aggregate_limited{
        config(input.size()), {}, limits, frame_input, dictionary_staging,
        frame_encoded};
    result = aggregate_limited.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz77BlockedHuffmanFrameStreamingEncoder, HandlesEmptyAndProtocolErrors) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lz77_blocked_huffman_stream_prefix_size> output{};
    Lz77BlockedHuffmanFrameStreamingEncoder empty{
        config(0), {}, {}, std::span<std::byte>{}, std::span<std::byte>{},
        unused};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced,
              lz77_blocked_huffman_stream_prefix_size);

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 32> dictionary_staging{};
    std::array<std::byte, 120> frame_encoded{};
    Lz77BlockedHuffmanFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    result = premature.process(
        std::span<const std::byte>{input}.first<1>(), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    Lz77BlockedHuffmanFrameStreamingEncoder reset{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

#include "frame/lz78_adaptive_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
    std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    const marc::core::DecoderLimits limits{};
    std::vector<std::byte> output(
        lz78_adaptive_huffman_stream_prefix_size);
    EXPECT_EQ(serialize_stream_header(
                  stream, limits,
                  std::span<std::byte, stream_header_size>{
                      output.data(), stream_header_size}),
              StreamHeaderError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lz78_parameters(
                  {}, limits,
                  std::span<std::byte,
                            marc::dictionary::internal::lz78_parameter_size>{
                      output.data() + stream_header_size,
                      marc::dictionary::internal::lz78_parameter_size}),
              marc::dictionary::internal::Lz78FormatError::none);

    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    std::array<std::byte, 16> staging{};
    std::uint64_t sequence{};
    std::uint64_t committed{};
    while (committed < input.size()) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(stream.frame_size,
                                    input.size() - committed));
        const auto raw = std::span<const std::byte>{input}.subspan(
            static_cast<std::size_t>(committed), count);
        const auto plan = plan_lz78_adaptive_huffman_frame(
            stream, {}, limits, sequence, committed, raw, entries, staging);
        EXPECT_EQ(plan.error,
                  Lz78AdaptiveHuffmanFrameValidationError::none);
        const auto offset = output.size();
        output.resize(offset + plan.serialized_size);
        EXPECT_EQ(encode_lz78_adaptive_huffman_frame(
                      stream, {}, limits, sequence, committed, raw, entries,
                      staging,
                      std::span<std::byte>{output}.subspan(
                          offset, plan.serialized_size)).error,
                  Lz78AdaptiveHuffmanFrameValidationError::none);
        committed += count;
        ++sequence;
    }
    return output;
}

} // namespace

TEST(Lz78AdaptiveHuffmanFrameStreamingEncoder,
     MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 16> dictionary_staging{};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    Lz78AdaptiveHuffmanFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded, entries};
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

TEST(Lz78AdaptiveHuffmanFrameStreamingEncoder,
     EmitsFullFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    Lz78AdaptiveHuffmanFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first<1>(), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lz78_adaptive_huffman_stream_prefix_size);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(1),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingEncoder,
     PreservesEndInputWhileDrainingPrefix) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    Lz78AdaptiveHuffmanFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    std::vector<std::byte> actual(expected.size());
    const auto first = encoder.process(
        input, std::span<std::byte>{actual}.first<1>(),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(first.status, marc::core::StreamStatus::need_output);
    ASSERT_EQ(first.input_consumed, 0U);
    ASSERT_EQ(first.output_produced, 1U);
    const auto second = encoder.process(
        input, std::span<std::byte>{actual}.subspan(1), 0);
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(second.input_consumed, input.size());
    EXPECT_EQ(second.output_produced, expected.size() - 1);
    EXPECT_EQ(actual, expected);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingEncoder,
     ReportsStorageAndAggregateErrors) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    std::array<std::byte, 4096> output{};

    Lz78AdaptiveHuffmanFrameStreamingEncoder short_input{
        config(input.size()), {}, {},
        std::span<std::byte>{frame_input}.first<1>(), staging, frame_encoded,
        entries};
    EXPECT_EQ(short_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);
    Lz78AdaptiveHuffmanFrameStreamingEncoder short_dictionary{
        config(input.size()), {}, {}, frame_input,
        std::span<std::byte>{staging}.first<15>(), frame_encoded, entries};
    EXPECT_EQ(short_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);
    Lz78AdaptiveHuffmanFrameStreamingEncoder short_entries{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        std::span<marc::dictionary::internal::Lz78EncoderEntry>{entries}
            .first<1>()};
    EXPECT_EQ(short_entries.process({}, {}, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    Lz78AdaptiveHuffmanFrameStreamingEncoder short_encoded{
        config(input.size()), {}, {}, frame_input, staging,
        std::span<std::byte>{frame_encoded}.first<1>(), entries};
    auto result = short_encoded.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    const auto plan = plan_lz78_adaptive_huffman_frame(
        config(input.size()), {}, {}, 0, 0,
        std::span<const std::byte>{input}.first<2>(), entries, staging);
    ASSERT_EQ(plan.error,
              Lz78AdaptiveHuffmanFrameValidationError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = 2 + plan.dictionary_size
        + plan.serialized_size
        + plan.encoder_entries
              * sizeof(marc::dictionary::internal::Lz78EncoderEntry)
        - 1;
    Lz78AdaptiveHuffmanFrameStreamingEncoder aggregate_limited{
        config(input.size()), {}, limits, frame_input, staging, frame_encoded,
        entries};
    result = aggregate_limited.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingEncoder,
     HandlesEmptyAndProtocolErrors) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lz78_adaptive_huffman_stream_prefix_size> output{};
    Lz78AdaptiveHuffmanFrameStreamingEncoder empty{
        config(0), {}, {}, {}, {}, unused, {}};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced,
              lz78_adaptive_huffman_stream_prefix_size);

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> entries{};
    Lz78AdaptiveHuffmanFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    result = premature.process(
        std::span<const std::byte>{input}.first<1>(), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    Lz78AdaptiveHuffmanFrameStreamingEncoder excess{
        config(1), {}, {}, frame_input, staging, frame_encoded, entries};
    result = excess.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    Lz78AdaptiveHuffmanFrameStreamingEncoder reset{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    Lz78AdaptiveHuffmanFrameStreamingEncoder unknown{
        config(input.size()), {}, {}, frame_input, staging, frame_encoded,
        entries};
    result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

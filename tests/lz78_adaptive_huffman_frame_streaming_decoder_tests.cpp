#include "frame/lz78_adaptive_huffman_frame_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array raw{
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

[[nodiscard]] std::vector<std::byte> encoded_stream(
    const std::span<const std::byte> input = raw) {
    const auto stream = config(input.size());
    const marc::core::DecoderLimits limits{};
    std::vector<std::byte> encoded(
        lz78_adaptive_huffman_stream_prefix_size);
    EXPECT_EQ(serialize_stream_header(
                  stream, limits,
                  std::span<std::byte, stream_header_size>{
                      encoded.data(), stream_header_size}),
              StreamHeaderError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lz78_parameters(
                  {}, limits,
                  std::span<std::byte,
                            marc::dictionary::internal::lz78_parameter_size>{
                      encoded.data() + stream_header_size,
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
        const auto frame_input = input.subspan(
            static_cast<std::size_t>(committed), count);
        const auto plan = plan_lz78_adaptive_huffman_frame(
            stream, {}, limits, sequence, committed, frame_input, entries,
            staging);
        EXPECT_EQ(plan.error,
                  Lz78AdaptiveHuffmanFrameValidationError::none);
        const auto offset = encoded.size();
        encoded.resize(offset + plan.serialized_size);
        EXPECT_EQ(encode_lz78_adaptive_huffman_frame(
                      stream, {}, limits, sequence, committed, frame_input,
                      entries, staging,
                      std::span<std::byte>{encoded}.subspan(
                          offset, plan.serialized_size)).error,
                  Lz78AdaptiveHuffmanFrameValidationError::none);
        committed += count;
        ++sequence;
    }
    return encoded;
}

[[nodiscard]] std::size_t first_frame_size(
    const std::span<const std::byte> encoded) {
    FrameHeader header{};
    const auto stream = config(raw.size());
    const std::span<const std::byte, frame_header_size> bytes{
        encoded.data() + lz78_adaptive_huffman_stream_prefix_size,
        frame_header_size};
    EXPECT_EQ(parse_frame_header(bytes, {stream, {}, 0, 0}, header),
              FrameHeaderError::none);
    return frame_header_size + header.block_descriptors_size
        + header.compressed_payload_size;
}

} // namespace

TEST(Lz78AdaptiveHuffmanFrameStreamingDecoder,
     HandlesOneByteInputAndOutputBoundaries) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 2048> frame_encoded{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    Lz78AdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, staging, decoded, phrases};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, encoded.size() - input_offset);
        const auto chunk = std::span<const std::byte>{encoded}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == encoded.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_TRUE(std::ranges::equal(actual, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingDecoder,
     RetainsEndInputWhileValidatedFramesDrain) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 2048> frame_encoded{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    Lz78AdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, staging, decoded, phrases};
    std::array<std::byte, raw.size()> actual{};
    std::size_t input_offset{};
    std::size_t output_offset{};
    marc::core::StreamStatus status{};
    do {
        const auto input = std::span<const std::byte>{encoded}.subspan(
            input_offset);
        const auto output = std::span<std::byte>{actual}.subspan(
            output_offset, std::min<std::size_t>(1,
                actual.size() - output_offset));
        const auto result = decoder.process(
            input, output,
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        output_offset += result.output_produced;
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_EQ(output_offset, raw.size());
    EXPECT_EQ(actual, raw);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingDecoder,
     LaterCorruptionIsStickyAndFrameAtomic) {
    auto encoded = encoded_stream();
    const auto second = lz78_adaptive_huffman_stream_prefix_size
        + first_frame_size(encoded);
    encoded[second + frame_header_size
            + marc::entropy::internal::adaptive_huffman_descriptor_size - 1]
        = std::byte{1};
    std::array<std::byte, 2048> frame_encoded{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    Lz78AdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, frame_encoded, staging, decoded, phrases};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 2U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{'B'});
    EXPECT_EQ(output[2], std::byte{0x5a});
    const auto repeated = decoder.process({}, {}, 0);
    EXPECT_EQ(repeated.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(repeated.error.byte_position, result.error.byte_position);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingDecoder,
     RejectsEveryTruncationTrailingDataAndAcceptsEmpty) {
    const auto encoded = encoded_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        std::array<std::byte, 2048> frame_encoded{};
        std::array<std::byte, 16> staging{};
        std::array<std::byte, 2> decoded{};
        std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
        Lz78AdaptiveHuffmanFrameStreamingDecoder decoder{
            {}, frame_encoded, staging, decoded, phrases};
        std::array<std::byte, raw.size()> output{};
        const auto result = decoder.process(
            std::span<const std::byte>{encoded}.first(size), output,
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream)
            << size;
    }

    auto trailing = encoded;
    trailing.push_back(std::byte{});
    std::array<std::byte, 2048> frame_encoded{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    Lz78AdaptiveHuffmanFrameStreamingDecoder trailing_decoder{
        {}, frame_encoded, staging, decoded, phrases};
    std::array<std::byte, raw.size()> output{};
    EXPECT_EQ(trailing_decoder.process(
                  trailing, output,
                  marc::core::flag_value(
                      marc::core::ProcessFlags::end_input)).error.code,
              marc::core::ErrorCode::malformed_stream);

    const auto empty_encoded = encoded_stream({});
    Lz78AdaptiveHuffmanFrameStreamingDecoder empty_decoder{
        {}, frame_encoded, staging, decoded, phrases};
    const auto empty = empty_decoder.process(
        empty_encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(empty.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(empty.input_consumed,
              lz78_adaptive_huffman_stream_prefix_size);
}

TEST(Lz78AdaptiveHuffmanFrameStreamingDecoder,
     ReportsWorkspaceAggregateAndProtocolErrors) {
    const auto encoded = encoded_stream();
    std::array<std::byte, 2048> frame_encoded{};
    std::array<std::byte, 16> staging{};
    std::array<std::byte, 2> decoded{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> phrases{};
    std::array<std::byte, raw.size()> output{};
    const auto flags =
        marc::core::flag_value(marc::core::ProcessFlags::end_input);

    Lz78AdaptiveHuffmanFrameStreamingDecoder short_frame{
        {}, std::span<std::byte>{frame_encoded}.first<1>(), staging, decoded,
        phrases};
    EXPECT_EQ(short_frame.process(encoded, output, flags).error.code,
              marc::core::ErrorCode::out_of_memory);
    Lz78AdaptiveHuffmanFrameStreamingDecoder short_tokens{
        {}, frame_encoded, std::span<std::byte>{staging}.first<15>(), decoded,
        phrases};
    EXPECT_EQ(short_tokens.process(encoded, output, flags).error.code,
              marc::core::ErrorCode::out_of_memory);
    Lz78AdaptiveHuffmanFrameStreamingDecoder short_raw{
        {}, frame_encoded, staging,
        std::span<std::byte>{decoded}.first<1>(), phrases};
    EXPECT_EQ(short_raw.process(encoded, output, flags).error.code,
              marc::core::ErrorCode::out_of_memory);
    Lz78AdaptiveHuffmanFrameStreamingDecoder short_phrases{
        {}, frame_encoded, staging, decoded,
        std::span<marc::dictionary::internal::Lz78PhraseEntry>{phrases}
            .first<1>()};
    EXPECT_EQ(short_phrases.process(encoded, output, flags).error.code,
              marc::core::ErrorCode::out_of_memory);

    FrameHeader first{};
    const std::span<const std::byte, frame_header_size> first_header{
        encoded.data() + lz78_adaptive_huffman_stream_prefix_size,
        frame_header_size};
    ASSERT_EQ(parse_frame_header(
                  first_header, {config(raw.size()), {}, 0, 0}, first),
              FrameHeaderError::none);
    const auto phrase_count =
        marc::dictionary::internal::lz78_validation_workspace_entries(
            first.dictionary_serialized_size, {});
    const std::uint64_t aggregate = frame_header_size
        + first.block_descriptors_size + first.compressed_payload_size
        + first.dictionary_serialized_size + first.uncompressed_size
        + phrase_count
              * sizeof(marc::dictionary::internal::Lz78PhraseEntry);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 16;
    limits.max_internal_buffered_bytes = aggregate - 1;
    Lz78AdaptiveHuffmanFrameStreamingDecoder limited{
        limits, frame_encoded, staging, decoded, phrases};
    EXPECT_EQ(limited.process(encoded, output, flags).error.code,
              marc::core::ErrorCode::limit_exceeded);

    Lz78AdaptiveHuffmanFrameStreamingDecoder reset{
        {}, frame_encoded, staging, decoded, phrases};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(
                              marc::core::ProcessFlags::reset_block))
                  .error.code,
              marc::core::ErrorCode::unsupported);
    Lz78AdaptiveHuffmanFrameStreamingDecoder unknown{
        {}, frame_encoded, staging, decoded, phrases};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              marc::core::ErrorCode::unsupported);
}

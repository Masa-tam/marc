#include "frame/lzss_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 6;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> encoded() {
    const auto stream = config();
    const auto plan = plan_lzss_stream(stream, {}, {}, raw);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzss_stream(stream, {}, {}, raw, output).error,
              LzssStreamCodecError::none);
    return output;
}

TEST(LzssFrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded();
    std::array<std::byte, 80> frame{};
    std::array<std::byte, 6> decoded_frame{};
    LzssFrameStreamingDecoder decoder{{}, frame, decoded_frame};
    std::vector<std::byte> output_bytes;
    std::size_t position{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - position);
        const auto chunk = std::span<const std::byte>{input}.subspan(position,
                                                                    count);
        const auto flags = position + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0) output_bytes.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, input.size());
    EXPECT_TRUE(std::ranges::equal(output_bytes, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssFrameStreamingDecoder, CommitsFirstFrameBeforeLaterCorruption) {
    auto input = encoded();
    constexpr std::size_t second_frame = 147;
    constexpr std::size_t second_payload = second_frame + frame_header_size;
    input[second_payload + 3] = std::byte{2};
    std::array<std::byte, 80> frame{};
    std::array<std::byte, 6> decoded_frame{};
    LzssFrameStreamingDecoder decoder{{}, frame, decoded_frame};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 6U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(6),
        std::span<const std::byte>{raw}.first(6)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(6),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzssFrameStreamingDecoder, ReportsWorkspaceAndTruncation) {
    const auto input = encoded();
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 6> decoded_frame{};
    LzssFrameStreamingDecoder short_decoder{{}, short_frame, decoded_frame};
    auto result = short_decoder.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 80> frame{};
    LzssFrameStreamingDecoder truncated{{}, frame, decoded_frame};
    std::array<std::byte, raw.size()> partial_output{};
    result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1),
        partial_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);

    std::array<std::byte, 5> short_decoded{};
    LzssFrameStreamingDecoder no_output{{}, frame, short_decoded};
    result = no_output.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(LzssFrameStreamingDecoder, HandlesEmptyAndRejectsTrailingData) {
    auto stream = config();
    stream.original_size = 0;
    const std::array<std::byte, 0> raw_input{};
    const auto plan = plan_lzss_stream(stream, {}, {}, raw_input);
    std::vector<std::byte> input(plan.serialized_size);
    ASSERT_EQ(encode_lzss_stream(stream, {}, {}, raw_input, input).error,
              LzssStreamCodecError::none);
    std::array<std::byte, 1> workspace{};
    LzssFrameStreamingDecoder empty{{}, workspace, workspace};
    auto result = empty.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    input.push_back(std::byte{});
    LzssFrameStreamingDecoder trailing{{}, workspace, workspace};
    result = trailing.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

} // namespace

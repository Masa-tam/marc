#include "frame/lz77_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'A'},
    std::byte{'B'}, std::byte{'C'}, std::byte{'X'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 4;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> encoded() {
    const auto stream = config();
    const auto plan = plan_lz77_stream(stream, {}, {}, raw);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz77_stream(stream, {}, {}, raw, output).error,
              Lz77StreamCodecError::none);
    return output;
}

TEST(Lz77FrameStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded();
    std::array<std::byte, 120> frame{};
    std::array<std::byte, 4> decoded_frame{};
    Lz77FrameStreamingDecoder decoder{{}, frame, decoded_frame};
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

TEST(Lz77FrameStreamingDecoder, CommitsFirstFrameBeforeLaterCorruption) {
    auto input = encoded();
    constexpr std::size_t second_frame = 80 + 56 + 64;
    constexpr std::size_t second_payload = second_frame + 56;
    input[second_payload + 4] = std::byte{1};
    std::array<std::byte, 120> frame{};
    std::array<std::byte, 4> decoded_frame{};
    Lz77FrameStreamingDecoder decoder{{}, frame, decoded_frame};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5A});
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 4U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(4),
        std::span<const std::byte>{raw}.first(4)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(4),
        [](std::byte value) { return value == std::byte{0x5A}; }));
}

TEST(Lz77FrameStreamingDecoder, ReportsWorkspaceAndTruncation) {
    const auto input = encoded();
    std::array<std::byte, 1> short_frame{};
    std::array<std::byte, 4> decoded_frame{};
    Lz77FrameStreamingDecoder short_decoder{{}, short_frame, decoded_frame};
    auto result = short_decoder.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 120> frame{};
    Lz77FrameStreamingDecoder truncated{{}, frame, decoded_frame};
    std::array<std::byte, raw.size()> partial_output{};
    result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1),
        partial_output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);

    std::array<std::byte, 3> short_decoded{};
    Lz77FrameStreamingDecoder no_output{{}, frame, short_decoded};
    result = no_output.process(input, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(Lz77FrameStreamingDecoder, HandlesEmptyAndRejectsTrailingData) {
    auto stream = config();
    stream.original_size = 0;
    const std::array<std::byte, 0> raw_input{};
    const auto plan = plan_lz77_stream(stream, {}, {}, raw_input);
    std::vector<std::byte> input(plan.serialized_size);
    ASSERT_EQ(encode_lz77_stream(stream, {}, {}, raw_input, input).error,
              Lz77StreamCodecError::none);
    std::array<std::byte, 1> workspace{};
    Lz77FrameStreamingDecoder empty{{}, workspace, workspace};
    auto result = empty.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    input.push_back(std::byte{});
    Lz77FrameStreamingDecoder trailing{{}, workspace, workspace};
    result = trailing.process(
        input, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
}

} // namespace

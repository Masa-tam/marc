#include "dictionary/lzw_encoder.hpp"
#include "dictionary/lzw_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

std::vector<std::byte> encoded(
    const std::span<const std::byte> input,
    const LzwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzwEncoderEntry> workspace(
        lzw_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lzw_code_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, LzwEncodeError::none);
    std::vector<std::byte> result(plan.output_size);
    EXPECT_EQ(encode_lzw_code_stream(
                  input, parameters, limits, workspace, result).error,
              LzwEncodeError::none);
    return result;
}

std::vector<std::byte> encoded(
    const std::string_view text,
    const LzwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    return encoded(bytes(text), parameters, limits);
}

std::vector<std::byte> boundary_input() {
    std::vector<std::byte> input(2048);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<std::byte>(
            (index * 37 + index / 7) & 0xffU);
    }
    return input;
}

TEST(LzwStreamingDecoder, HandlesEmptyAndEndedCalls) {
    LzwStreamingDecoder decoder{{}, 0, {}, {}};
    auto result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_input);
    result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzwStreamingDecoder, DecodesOneByteInputAndOutput) {
    constexpr std::string_view raw = "ABABABA";
    const auto input = encoded(raw);
    std::array<LzwPhraseEntry, raw.size() - 1> dictionary{};
    LzwStreamingDecoder decoder{{}, raw.size(), {}, dictionary};
    std::vector<std::byte> decoded;
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
        if (result.output_produced != 0) decoded.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, input.size());
    EXPECT_EQ(decoded, bytes(raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzwStreamingDecoder, PreservesEndInputWhileDrainingPhrase) {
    constexpr std::string_view raw = "ABABABA";
    const auto input = encoded(raw);
    std::array<LzwPhraseEntry, raw.size() - 1> dictionary{};
    LzwStreamingDecoder decoder{{}, raw.size(), {}, dictionary};
    std::array<std::byte, raw.size()> output{};
    auto result = decoder.process(
        input, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    std::size_t input_position = result.input_consumed;
    std::size_t output_position = result.output_produced;
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        result = decoder.process(
            std::span<const std::byte>{input}.subspan(input_position),
            std::span<std::byte>{output}.subspan(output_position),
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_position += result.input_consumed;
        output_position += result.output_produced;
    }
    EXPECT_EQ(input_position, input.size());
    EXPECT_EQ(output_position, output.size());
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes(raw));
}

TEST(LzwStreamingDecoder, AcceptsEndInputWithZeroFinalBytes) {
    const auto input = encoded("AAA");
    std::array<LzwPhraseEntry, 2> dictionary{};
    std::array<std::byte, 3> output{};
    LzwStreamingDecoder decoder{{}, output.size(), {}, dictionary};
    auto result = decoder.process(input, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.input_consumed, input.size());
    EXPECT_EQ(result.output_produced, output.size());
    result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes("AAA"));
}

TEST(LzwStreamingDecoder, CrossesWidthBoundaryWithSplitCodes) {
    const auto raw = boundary_input();
    const auto input = encoded(raw);
    std::vector<LzwPhraseEntry> dictionary(raw.size() - 1);
    LzwStreamingDecoder decoder{{}, raw.size(), {}, dictionary};
    std::vector<std::byte> output(raw.size());
    std::size_t input_position{};
    std::size_t output_position{};
    marc::core::StreamStatus status{};
    do {
        const auto input_count = std::min<std::size_t>(
            1, input.size() - input_position);
        const auto output_count = std::min<std::size_t>(
            3, output.size() - output_position);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            input_position, input_count);
        const auto flags = input_position + input_count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(
            chunk, std::span<std::byte>{output}.subspan(
                       output_position, output_count),
            flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output_count));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_position += result.input_consumed;
        output_position += result.output_produced;
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_position, input.size());
    EXPECT_EQ(output_position, output.size());
    EXPECT_EQ(output, raw);
}

TEST(LzwStreamingDecoder, DecodesAfterDictionaryFreeze) {
    const auto raw = boundary_input();
    LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 256;
    const auto input = encoded(raw, parameters, limits);
    std::array<LzwPhraseEntry, 256> dictionary{};
    std::vector<std::byte> output(raw.size());
    LzwStreamingDecoder decoder{
        parameters, raw.size(), limits, dictionary};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(output, raw);
}

TEST(LzwStreamingDecoder, ReportsTruncationPaddingTrailingAndForwardCode) {
    auto input = encoded("AAA");
    std::array<std::byte, 3> output{};
    std::array<LzwPhraseEntry, 2> dictionary1{};
    LzwStreamingDecoder truncated{{}, 3, {}, dictionary1};
    auto result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);

    input = encoded("A");
    input[1] = std::byte{0x02};
    std::array<LzwPhraseEntry, 1> dictionary2{};
    LzwStreamingDecoder padding{{}, 1, {}, dictionary2};
    result = padding.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);

    input = encoded("A");
    input.push_back(std::byte{});
    std::array<LzwPhraseEntry, 1> dictionary3{};
    LzwStreamingDecoder trailing{{}, 1, {}, dictionary3};
    result = trailing.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);

    constexpr std::array forward{
        std::byte{0x41}, std::byte{0x02}, std::byte{0x02}};
    std::array<LzwPhraseEntry, 1> dictionary4{};
    LzwStreamingDecoder invalid{{}, 2, {}, dictionary4};
    output.fill(std::byte{0xcc});
    result = invalid.process(
        forward, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xcc});
}

TEST(LzwStreamingDecoder, RejectsUnsupportedFlagsAndInvalidConstruction) {
    std::array<LzwPhraseEntry, 1> dictionary{};
    LzwStreamingDecoder decoder{{}, 2, {}, dictionary};
    auto result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    LzwStreamingDecoder invalid{{}, 1, limits, dictionary};
    result = invalid.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    LzwStreamingDecoder no_dictionary{{}, 2, {}, {}};
    result = no_dictionary.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    LzwParameters parameters{};
    parameters.maximum_code_width = 8;
    LzwStreamingDecoder invalid_parameters{
        parameters, 1, {}, dictionary};
    result = invalid_parameters.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
}

TEST(LzwStreamingDecoder, EnforcesCumulativeSerializedLimitBeforeInput) {
    const auto input = encoded("ABABABA");
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = input.size() - 1;
    std::array<LzwPhraseEntry, 6> dictionary{};
    std::array<std::byte, 7> output{};
    LzwStreamingDecoder decoder{{}, output.size(), limits, dictionary};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
}

} // namespace

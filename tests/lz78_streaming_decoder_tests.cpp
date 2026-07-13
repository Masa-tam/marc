#include "dictionary/lz78_encoder.hpp"
#include "dictionary/lz78_streaming_decoder.hpp"

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
    const std::string_view text, const Lz78Parameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    const auto input = bytes(text);
    std::vector<Lz78EncoderEntry> workspace(
        lz78_encoder_workspace_entries(input.size(), parameters));
    const auto plan = plan_lz78_token_stream(
        input, parameters, limits, workspace);
    EXPECT_EQ(plan.error, Lz78EncodeError::none);
    std::vector<std::byte> result(plan.output_size);
    EXPECT_EQ(encode_lz78_token_stream(
                  input, parameters, limits, workspace, result).error,
              Lz78EncodeError::none);
    return result;
}

TEST(Lz78StreamingDecoder, DecodesOneByteInputAndOutput) {
    constexpr std::string_view raw = "AABABCABC";
    const auto input = encoded(raw);
    std::array<Lz78PhraseEntry, raw.size()> dictionary{};
    Lz78StreamingDecoder decoder{{}, raw.size(), {}, dictionary};
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

TEST(Lz78StreamingDecoder, PreservesEndInputWhileDrainingPhrase) {
    constexpr std::string_view raw = "AABABCABC";
    const auto input = encoded(raw);
    std::array<Lz78PhraseEntry, raw.size()> dictionary{};
    Lz78StreamingDecoder decoder{{}, raw.size(), {}, dictionary};
    std::array<std::byte, raw.size()> output{};
    auto result = decoder.process(
        input, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 2 * lz78_token_size);
    std::size_t input_position = result.input_consumed;
    std::size_t output_position = result.output_produced;
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        const auto remaining = std::span<const std::byte>{input}.subspan(
            input_position);
        result = decoder.process(
            remaining, std::span<std::byte>{output}.subspan(output_position),
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_position += result.input_consumed;
        output_position += result.output_produced;
    }
    EXPECT_EQ(input_position, input.size());
    EXPECT_EQ(output_position, output.size());
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes(raw));
}

TEST(Lz78StreamingDecoder, AcceptsEndInputWithZeroFinalBytes) {
    const auto input = encoded("ABAB");
    std::array<Lz78PhraseEntry, 4> dictionary{};
    std::array<std::byte, 4> output{};
    Lz78StreamingDecoder decoder{{}, 4, {}, dictionary};
    auto result = decoder.process(input, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.input_consumed, input.size());
    EXPECT_EQ(result.output_produced, output.size());
    result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes("ABAB"));
}

TEST(Lz78StreamingDecoder, DecodesWithFrozenDictionary) {
    Lz78Parameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    const auto input = encoded("AAA", parameters, limits);
    std::array<Lz78PhraseEntry, 1> dictionary{};
    std::array<std::byte, 3> output{};
    Lz78StreamingDecoder decoder{parameters, 3, limits, dictionary};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes("AAA"));
}

TEST(Lz78StreamingDecoder, ReportsTruncationTrailingAndForwardReference) {
    auto input = encoded("AA");
    std::array<std::byte, 2> output{};
    std::array<Lz78PhraseEntry, 2> dictionary1{};
    Lz78StreamingDecoder truncated{{}, 2, {}, dictionary1};
    auto result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);

    std::array<Lz78PhraseEntry, 2> dictionary2{};
    Lz78StreamingDecoder trailing{{}, 2, {}, dictionary2};
    input.push_back(std::byte{});
    result = trailing.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 2U);

    input = encoded("AB");
    input[lz78_token_size + 4] = std::byte{2};
    std::array<Lz78PhraseEntry, 2> dictionary3{};
    Lz78StreamingDecoder invalid{{}, 2, {}, dictionary3};
    output.fill(std::byte{0xcc});
    result = invalid.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xcc});
}

TEST(Lz78StreamingDecoder, RejectsUnsupportedFlagsAndInvalidConstruction) {
    std::array<Lz78PhraseEntry, 1> dictionary{};
    Lz78StreamingDecoder decoder{{}, 1, {}, dictionary};
    auto result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    Lz78StreamingDecoder invalid{{}, 1, limits, dictionary};
    result = invalid.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    Lz78StreamingDecoder no_dictionary{{}, 1, {}, {}};
    result = no_dictionary.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(Lz78StreamingDecoder, EnforcesCumulativeSerializedLimitBeforeInput) {
    const auto input = encoded("AB");
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = lz78_token_size;
    std::array<Lz78PhraseEntry, 2> dictionary{};
    std::array<std::byte, 2> output{};
    Lz78StreamingDecoder decoder{{}, 2, limits, dictionary};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
}

} // namespace

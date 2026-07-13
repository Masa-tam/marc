#include "dictionary/lz77_encoder.hpp"
#include "dictionary/lz77_streaming_decoder.hpp"

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

std::vector<std::byte> encoded(const std::string_view text) {
    const auto input = bytes(text);
    const auto plan = plan_lz77_token_stream(input, {}, {});
    std::vector<std::byte> result(plan.output_size);
    EXPECT_EQ(encode_lz77_token_stream(input, {}, {}, result).error,
              Lz77EncodeError::none);
    return result;
}

TEST(Lz77StreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto input = encoded("ABCABCXAAAA");
    std::array<std::byte, 11> history{};
    Lz77StreamingDecoder decoder{{}, 11, {}, history};
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
    EXPECT_EQ(decoded, bytes("ABCABCXAAAA"));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(Lz77StreamingDecoder, PreservesEndInputWhileDraining) {
    const auto input = encoded("AAAA");
    std::array<std::byte, 4> history{};
    Lz77StreamingDecoder decoder{{}, 4, {}, history};
    std::array<std::byte, 4> output{};
    auto result = decoder.process(
        input, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, input.size());
    std::size_t produced = result.output_produced;
    result = decoder.process({}, std::span<std::byte>{output}.subspan(produced),
                             0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    produced += result.output_produced;
    EXPECT_EQ(produced, output.size());
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes("AAAA"));
}

TEST(Lz77StreamingDecoder, WrapsCallerOwnedHistoryRing) {
    const auto raw = bytes("ABCABCABCX");
    Lz77Parameters parameters{};
    parameters.window_size = 3;
    const auto plan = plan_lz77_token_stream(raw, parameters, {});
    ASSERT_EQ(plan.error, Lz77EncodeError::none);
    std::vector<std::byte> input(plan.output_size);
    ASSERT_EQ(encode_lz77_token_stream(raw, parameters, {}, input).error,
              Lz77EncodeError::none);
    std::array<std::byte, 3> history{};
    std::array<std::byte, 10> output{};
    Lz77StreamingDecoder decoder{parameters, raw.size(), {}, history};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(std::vector(output.begin(), output.end()), raw);
}

TEST(Lz77StreamingDecoder, ReportsTruncationTrailingDataAndBadReference) {
    auto input = encoded("AAAA");
    std::array<std::byte, 4> output{};
    std::array<std::byte, 4> history1{};
    Lz77StreamingDecoder truncated{{}, 4, {}, history1};
    auto result = truncated.process(
        std::span<const std::byte>{input}.first(input.size() - 1), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);

    std::array<std::byte, 4> history2{};
    Lz77StreamingDecoder trailing{{}, 4, {}, history2};
    input.push_back(std::byte{});
    result = trailing.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 4U);

    input = encoded("AAAA");
    input[16 + 4] = std::byte{2};
    std::array<std::byte, 4> history3{};
    Lz77StreamingDecoder invalid{{}, 4, {}, history3};
    output.fill(std::byte{0xCC});
    result = invalid.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xCC});
}

TEST(Lz77StreamingDecoder, RejectsUnsupportedFlagsAndInvalidConstruction) {
    std::array<std::byte, 1> history{};
    Lz77StreamingDecoder decoder{{}, 1, {}, history};
    auto result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    marc::core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_total_output_size + 1;
    Lz77StreamingDecoder invalid{{}, 1, limits, history};
    result = invalid.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    Lz77StreamingDecoder no_history{{}, 1, {}, {}};
    result = no_history.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
}

TEST(Lz77StreamingDecoder, EnforcesCumulativeSerializedLimitBeforeInput) {
    const auto input = encoded("AB");
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_serialized_size = lz77_token_size;
    std::array<std::byte, 2> history{};
    std::array<std::byte, 2> output{};
    Lz77StreamingDecoder decoder{{}, 2, limits, history};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
}

} // namespace

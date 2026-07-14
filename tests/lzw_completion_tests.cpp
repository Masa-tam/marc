#include "frame/lzw_stream.hpp"
#include "frame/lzw_streaming_decoder.hpp"
#include "frame/lzw_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::size_t test_frame_size = 64;
constexpr std::size_t maximum_frame_payload = test_frame_size * 2;
constexpr std::size_t maximum_encoded_frame =
    frame_header_size + maximum_frame_payload;

StreamHeader config(const std::size_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = test_frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encode_reference(
    const std::span<const std::byte> input) {
    std::array<marc::dictionary::internal::LzwEncoderEntry,
               test_frame_size - 1>
        dictionary{};
    const auto stream = config(input.size());
    const auto plan = plan_lzw_stream(stream, {}, {}, input, dictionary);
    EXPECT_EQ(plan.error, LzwStreamCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzw_stream(
                  stream, {}, {}, input, dictionary, encoded).error,
              LzwStreamCodecError::none);
    return encoded;
}

void expect_one_shot_round_trip(const std::span<const std::byte> input) {
    const auto first = encode_reference(input);
    const auto second = encode_reference(input);
    EXPECT_EQ(second, first);

    std::array<marc::dictionary::internal::LzwPhraseEntry,
               test_frame_size - 1>
        dictionary{};
    std::vector<std::byte> decoded(input.size());
    StreamHeader stream{};
    marc::dictionary::internal::LzwParameters parameters{};
    const auto result = decode_lzw_stream(
        first, {}, dictionary, decoded, stream, parameters);
    ASSERT_EQ(result.error, LzwStreamCodecError::none);
    EXPECT_TRUE(std::ranges::equal(decoded, input));
    EXPECT_EQ(stream.original_size, input.size());
    EXPECT_EQ(stream.frame_size, test_frame_size);
    EXPECT_EQ(parameters.maximum_code_width, 16U);
}

std::vector<std::byte> generated_bytes(const std::size_t size,
                                       std::uint32_t state) {
    std::vector<std::byte> result(size);
    for (auto& value : result) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        value = static_cast<std::byte>(state >> 24);
    }
    return result;
}

void expect_chunked_round_trip(const std::span<const std::byte> input,
                               const std::size_t input_capacity,
                               const std::size_t output_capacity) {
    const auto expected = encode_reference(input);
    std::array<std::byte, test_frame_size> frame_input{};
    std::array<std::byte, maximum_encoded_frame> frame_encoded{};
    std::array<marc::dictionary::internal::LzwEncoderEntry,
               test_frame_size - 1>
        encoder_dictionary{};
    LzwFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, frame_encoded,
        encoder_dictionary};
    std::vector<std::byte> actual(expected.size());
    std::size_t consumed{};
    std::size_t produced{};
    for (std::size_t call = 0;
         call < input.size() + expected.size() + 32; ++call) {
        const auto available_input = std::min(
            input_capacity, input.size() - consumed);
        const auto chunk = input.subspan(consumed, available_input);
        const auto flags = consumed + available_input == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : UINT32_C(0);
        const auto available_output = std::min(
            output_capacity, actual.size() - produced);
        const auto result = encoder.process(
            chunk,
            std::span<std::byte>{actual}.subspan(produced, available_output),
            flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), available_output));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        consumed += result.input_consumed;
        produced += result.output_produced;
        if (result.status == marc::core::StreamStatus::end_of_stream) break;
    }
    ASSERT_EQ(consumed, input.size());
    ASSERT_EQ(produced, expected.size());
    EXPECT_EQ(actual, expected);

    std::array<std::byte, maximum_encoded_frame> decoder_encoded{};
    std::array<std::byte, test_frame_size> decoder_decoded{};
    std::array<marc::dictionary::internal::LzwPhraseEntry,
               test_frame_size - 1>
        decoder_dictionary{};
    LzwFrameStreamingDecoder decoder{
        {}, decoder_encoded, decoder_decoded, decoder_dictionary};
    std::vector<std::byte> decoded(input.size());
    consumed = 0;
    produced = 0;
    for (std::size_t call = 0;
         call < expected.size() + input.size() + 32; ++call) {
        const auto available_input = std::min(
            input_capacity, expected.size() - consumed);
        const auto chunk = std::span<const std::byte>{expected}.subspan(
            consumed, available_input);
        const auto flags = consumed + available_input == expected.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : UINT32_C(0);
        const auto available_output = std::min(
            output_capacity, decoded.size() - produced);
        const auto result = decoder.process(
            chunk,
            std::span<std::byte>{decoded}.subspan(produced, available_output),
            flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), available_output));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        consumed += result.input_consumed;
        produced += result.output_produced;
        if (result.status == marc::core::StreamStatus::end_of_stream) break;
    }
    ASSERT_EQ(consumed, expected.size());
    ASSERT_EQ(produced, input.size());
    EXPECT_TRUE(std::ranges::equal(decoded, input));
}

TEST(LzwCompletion, RequiredDataClassesRoundTripDeterministically) {
    const std::array<std::size_t, 3> boundary_sizes{63, 64, 65};
    for (const auto size : boundary_sizes)
        expect_one_shot_round_trip(generated_bytes(size, 1));

    expect_one_shot_round_trip({});
    for (std::uint32_t value = 0; value < 256; ++value) {
        const std::array one_byte{static_cast<std::byte>(value)};
        expect_one_shot_round_trip(one_byte);
    }

    std::vector<std::byte> all_values(256);
    for (std::size_t value = 0; value < all_values.size(); ++value)
        all_values[value] = static_cast<std::byte>(value);
    expect_one_shot_round_trip(all_values);

    std::vector<std::byte> zeros(257, std::byte{});
    expect_one_shot_round_trip(zeros);

    std::vector<std::byte> repeated_pattern(259);
    constexpr std::array pattern{
        std::byte{0x00}, std::byte{0xff}, std::byte{0x55}, std::byte{0xaa}};
    for (std::size_t index = 0; index < repeated_pattern.size(); ++index)
        repeated_pattern[index] = pattern[index % pattern.size()];
    expect_one_shot_round_trip(repeated_pattern);

    // Deterministic high-entropy data models an already-compressed payload.
    expect_one_shot_round_trip(generated_bytes(513, UINT32_C(0xc001d00d)));
}

TEST(LzwCompletion, MultiFrameStreamIsIndependentOfChunking) {
    const auto input = generated_bytes(193, UINT32_C(0x6d617263));
    expect_chunked_round_trip(input, 1, 1);
    expect_chunked_round_trip(input, 7, 5);
    expect_chunked_round_trip(input, 13, 17);
}

} // namespace

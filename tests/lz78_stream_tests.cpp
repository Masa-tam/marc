#include "frame/lz78_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array repeated_a{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config(const std::uint64_t size,
                    const std::uint32_t frame_size = 3) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encoded_repeated_a() {
    const auto stream = config(repeated_a.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    const auto plan = plan_lz78_stream(
        stream, {}, {}, repeated_a, dictionary);
    EXPECT_EQ(plan.error, Lz78StreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz78_stream(
                  stream, {}, {}, repeated_a, dictionary, output).error,
              Lz78StreamCodecError::none);
    return output;
}

TEST(Lz78Stream, RoundTripsMultipleResetFrames) {
    const auto stream = config(repeated_a.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3>
        encoder_dictionary{};
    const auto plan = plan_lz78_stream(
        stream, {}, {}, repeated_a, encoder_dictionary);
    ASSERT_EQ(plan.error, Lz78StreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 224U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(80 + 56, 16),
        std::span<const std::byte>{encoded}.subspan(152 + 56, 16)));

    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decoder_dictionary{};
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader decoded_stream{};
    marc::dictionary::internal::Lz78Parameters decoded_parameters{};
    const auto result = decode_lz78_stream(
        encoded, {}, decoder_dictionary, output, decoded_stream,
        decoded_parameters);
    EXPECT_EQ(result.error, Lz78StreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_parameters.maximum_entries, 65536U);
}

TEST(Lz78Stream, EmptyInputContainsHeaderAndParameters) {
    const auto stream = config(0);
    const std::array<std::byte, 0> input{};
    const auto plan = plan_lz78_stream(stream, {}, {}, input, {});
    ASSERT_EQ(plan.error, Lz78StreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(encode_lz78_stream(
                  stream, {}, {}, input, {}, encoded).error,
              Lz78StreamCodecError::none);
    StreamHeader decoded_stream{};
    marc::dictionary::internal::Lz78Parameters decoded_parameters{};
    EXPECT_EQ(decode_lz78_stream(
                  encoded, {}, {}, {}, decoded_stream, decoded_parameters).error,
              Lz78StreamCodecError::none);
}

TEST(Lz78Stream, PlanningAndLaterCorruptionAreAtomic) {
    const auto stream = config(repeated_a.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3>
        encoder_dictionary{};
    const auto plan = plan_lz78_stream(
        stream, {}, {}, repeated_a, encoder_dictionary);
    std::vector<std::byte> short_output(
        plan.serialized_size - 1, std::byte{0x5a});
    EXPECT_EQ(encode_lz78_stream(
                  stream, {}, {}, repeated_a, encoder_dictionary, short_output)
                  .error,
              Lz78StreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    auto encoded = encoded_repeated_a();
    encoded[152 + 56 + 8 + 4] = std::byte{2};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decoder_dictionary{};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader decoded_stream{};
    decoded_stream.original_size = 99;
    marc::dictionary::internal::Lz78Parameters decoded_parameters{};
    decoded_parameters.maximum_entries = 9;
    const auto result = decode_lz78_stream(
        encoded, {}, decoder_dictionary, output, decoded_stream,
        decoded_parameters);
    EXPECT_EQ(result.error, Lz78StreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(decoded_stream.original_size, 99U);
    EXPECT_EQ(decoded_parameters.maximum_entries, 9U);
}

TEST(Lz78Stream, RejectsTruncationTrailingInvalidParametersAndWorkspace) {
    const auto encoded = encoded_repeated_a();
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader stream{};
    marc::dictionary::internal::Lz78Parameters parameters{};
    EXPECT_EQ(decode_lz78_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, dictionary, output, stream, parameters).error,
              Lz78StreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{});
    EXPECT_EQ(decode_lz78_stream(
                  extended, {}, dictionary, output, stream, parameters).error,
              Lz78StreamCodecError::trailing_stream_bytes);
    auto invalid = encoded;
    std::fill(invalid.begin() + stream_header_size,
              invalid.begin() + stream_header_size + 4, std::byte{});
    EXPECT_EQ(decode_lz78_stream(
                  invalid, {}, dictionary, output, stream, parameters).error,
              Lz78StreamCodecError::invalid_parameters);
    EXPECT_EQ(decode_lz78_stream(
                  encoded, {}, {}, output, stream, parameters).error,
              Lz78StreamCodecError::frame_error);

    const auto configured = config(repeated_a.size());
    EXPECT_EQ(plan_lz78_stream(
                  configured, {}, {}, repeated_a, {}).error,
              Lz78StreamCodecError::frame_error);
}

TEST(Lz78Stream, RejectsInputAndOutputSizeMismatch) {
    auto stream = config(repeated_a.size() + 1);
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    EXPECT_EQ(plan_lz78_stream(
                  stream, {}, {}, repeated_a, dictionary).error,
              Lz78StreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decode_dictionary{};
    marc::dictionary::internal::Lz78Parameters parameters{};
    EXPECT_EQ(decode_lz78_stream(
                  encoded, {}, decode_dictionary, output, stream, parameters)
                  .error,
              Lz78StreamCodecError::output_too_small);
}

} // namespace

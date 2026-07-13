#include "frame/lz77_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array repeated_a{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config(const std::uint64_t size,
                    const std::uint32_t frame_size = 2) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encoded_repeated_a() {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lz77_stream(stream, {}, {}, repeated_a);
    EXPECT_EQ(plan.error, Lz77StreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lz77_stream(stream, {}, {}, repeated_a, output).error,
              Lz77StreamCodecError::none);
    return output;
}

TEST(Lz77Stream, RoundTripsMultipleResetFrames) {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lz77_stream(stream, {}, {}, repeated_a);
    ASSERT_EQ(plan.error, Lz77StreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 256U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(80 + 56, 32),
        std::span<const std::byte>{encoded}.subspan(168 + 56, 32)));
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader decoded_stream{};
    marc::dictionary::internal::Lz77Parameters decoded_parameters{};
    const auto result = decode_lz77_stream(
        encoded, {}, output, decoded_stream, decoded_parameters);
    EXPECT_EQ(result.error, Lz77StreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_parameters.window_size, 65536U);
}

TEST(Lz77Stream, EmptyInputContainsHeaderAndParameters) {
    const auto stream = config(0);
    const std::array<std::byte, 0> input{};
    const auto plan = plan_lz77_stream(stream, {}, {}, input);
    ASSERT_EQ(plan.error, Lz77StreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(encode_lz77_stream(stream, {}, {}, input, encoded).error,
              Lz77StreamCodecError::none);
    StreamHeader decoded_stream{};
    marc::dictionary::internal::Lz77Parameters decoded_parameters{};
    EXPECT_EQ(decode_lz77_stream(encoded, {}, {}, decoded_stream,
                                 decoded_parameters).error,
              Lz77StreamCodecError::none);
}

TEST(Lz77Stream, PlanningAndLaterCorruptionAreAtomic) {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lz77_stream(stream, {}, {}, repeated_a);
    std::vector<std::byte> short_output(plan.serialized_size - 1,
                                        std::byte{0x5A});
    EXPECT_EQ(encode_lz77_stream(stream, {}, {}, repeated_a, short_output).error,
              Lz77StreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output,
        [](std::byte value) { return value == std::byte{0x5A}; }));

    auto encoded = encoded_repeated_a();
    encoded[168 + 56 + 4] = std::byte{1};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5A});
    StreamHeader decoded_stream{};
    marc::dictionary::internal::Lz77Parameters decoded_parameters{};
    const auto result = decode_lz77_stream(
        encoded, {}, output, decoded_stream, decoded_parameters);
    EXPECT_EQ(result.error, Lz77StreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](std::byte value) { return value == std::byte{0x5A}; }));
}

TEST(Lz77Stream, RejectsTruncationTrailingAndInvalidParameters) {
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader stream{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    EXPECT_EQ(decode_lz77_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, output, stream, parameters).error,
              Lz77StreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{});
    EXPECT_EQ(decode_lz77_stream(extended, {}, output, stream, parameters).error,
              Lz77StreamCodecError::trailing_stream_bytes);
    auto invalid = encoded;
    std::fill(invalid.begin() + stream_header_size,
              invalid.begin() + stream_header_size + 4, std::byte{});
    EXPECT_EQ(decode_lz77_stream(invalid, {}, output, stream, parameters).error,
              Lz77StreamCodecError::invalid_parameters);
}

TEST(Lz77Stream, RejectsInputAndOutputSizeMismatch) {
    auto stream = config(repeated_a.size() + 1);
    EXPECT_EQ(plan_lz77_stream(stream, {}, {}, repeated_a).error,
              Lz77StreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    marc::dictionary::internal::Lz77Parameters parameters{};
    EXPECT_EQ(decode_lz77_stream(encoded, {}, output, stream, parameters).error,
              Lz77StreamCodecError::output_too_small);
}

} // namespace

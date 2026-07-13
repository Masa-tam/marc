#include "frame/lzss_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array repeated_a{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config(const std::uint64_t size,
                    const std::uint32_t frame_size = 6) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encoded_repeated_a() {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lzss_stream(stream, {}, {}, repeated_a);
    EXPECT_EQ(plan.error, LzssStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzss_stream(stream, {}, {}, repeated_a, output).error,
              LzssStreamCodecError::none);
    return output;
}

TEST(LzssStream, RoundTripsMultipleResetFrames) {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lzss_stream(stream, {}, {}, repeated_a);
    ASSERT_EQ(plan.error, LzssStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 214U);
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_repeated_a();
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(80 + 56, 11),
        std::span<const std::byte>{encoded}.subspan(147 + 56, 11)));
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader decoded_stream{};
    marc::dictionary::internal::LzssParameters decoded_parameters{};
    const auto result = decode_lzss_stream(
        encoded, {}, output, decoded_stream, decoded_parameters);
    EXPECT_EQ(result.error, LzssStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, repeated_a);
    EXPECT_EQ(decoded_parameters.window_size, 65536U);
}

TEST(LzssStream, EmptyInputContainsHeaderAndParameters) {
    const auto stream = config(0);
    const std::array<std::byte, 0> input{};
    const auto plan = plan_lzss_stream(stream, {}, {}, input);
    ASSERT_EQ(plan.error, LzssStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(encode_lzss_stream(stream, {}, {}, input, encoded).error,
              LzssStreamCodecError::none);
    StreamHeader decoded_stream{};
    marc::dictionary::internal::LzssParameters decoded_parameters{};
    EXPECT_EQ(decode_lzss_stream(encoded, {}, {}, decoded_stream,
                                 decoded_parameters).error,
              LzssStreamCodecError::none);
}

TEST(LzssStream, PlanningAndLaterCorruptionAreAtomic) {
    const auto stream = config(repeated_a.size());
    const auto plan = plan_lzss_stream(stream, {}, {}, repeated_a);
    std::vector<std::byte> short_output(plan.serialized_size - 1,
                                        std::byte{0x5a});
    EXPECT_EQ(encode_lzss_stream(stream, {}, {}, repeated_a, short_output).error,
              LzssStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    auto encoded = encoded_repeated_a();
    encoded[147 + 56 + 3] = std::byte{2};
    std::array<std::byte, repeated_a.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader decoded_stream{};
    marc::dictionary::internal::LzssParameters decoded_parameters{};
    const auto result = decode_lzss_stream(
        encoded, {}, output, decoded_stream, decoded_parameters);
    EXPECT_EQ(result.error, LzssStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzssStream, RejectsTruncationTrailingAndInvalidParameters) {
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size()> output{};
    StreamHeader stream{};
    marc::dictionary::internal::LzssParameters parameters{};
    EXPECT_EQ(decode_lzss_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, output, stream, parameters).error,
              LzssStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{});
    EXPECT_EQ(decode_lzss_stream(extended, {}, output, stream, parameters).error,
              LzssStreamCodecError::trailing_stream_bytes);
    auto invalid = encoded;
    std::fill(invalid.begin() + stream_header_size,
              invalid.begin() + stream_header_size + 4, std::byte{});
    EXPECT_EQ(decode_lzss_stream(invalid, {}, output, stream, parameters).error,
              LzssStreamCodecError::invalid_parameters);
}

TEST(LzssStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = config(repeated_a.size() + 1);
    EXPECT_EQ(plan_lzss_stream(stream, {}, {}, repeated_a).error,
              LzssStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_repeated_a();
    std::array<std::byte, repeated_a.size() - 1> output{};
    marc::dictionary::internal::LzssParameters parameters{};
    EXPECT_EQ(decode_lzss_stream(encoded, {}, output, stream, parameters).error,
              LzssStreamCodecError::output_too_small);
}

} // namespace

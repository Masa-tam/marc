#include "frame/lzmw_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw_abab{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};

StreamHeader config(const std::uint64_t size,
                    const std::uint32_t frame_size = 2) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encoded_abab() {
    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw_abab, dictionary);
    EXPECT_EQ(plan.error, LzmwStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzmw_stream(
                  stream, {}, {}, raw_abab, dictionary, output).error,
              LzmwStreamCodecError::none);
    return output;
}

TEST(LzmwStream, EmitsDocumentedMultipleResetFrameVector) {
    constexpr std::array prefix{
        std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43},
        std::byte{0x01}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x40}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x06}, std::byte{0}, std::byte{0x01}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x02}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x04}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0x01}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    constexpr std::array frame{
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x02}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x08}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x08}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x42}, std::byte{0}, std::byte{0}, std::byte{0}};
    std::vector<std::byte> expected(prefix.begin(), prefix.end());
    expected.insert(expected.end(), frame.begin(), frame.end());
    expected.insert(expected.end(), frame.begin(), frame.end());
    expected[80 + frame.size() + 8] = std::byte{1};
    ASSERT_EQ(expected.size(), 208U);

    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> encoder{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw_abab, encoder);
    ASSERT_EQ(plan.error, LzmwStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, expected.size());
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_abab();
    EXPECT_EQ(encoded, expected);

    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    StreamHeader decoded_stream{};
    marc::dictionary::internal::LzmwParameters parameters{};
    const auto result = decode_lzmw_stream(
        encoded, {}, phrases, expansion, output, decoded_stream, parameters);
    EXPECT_EQ(result.error, LzmwStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, raw_abab);
    EXPECT_EQ(parameters.maximum_entries, 65536U);
}

TEST(LzmwStream, EmptyInputContainsHeaderAndParameters) {
    const auto stream = config(0);
    const std::array<std::byte, 0> input{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, input, {});
    ASSERT_EQ(plan.error, LzmwStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(encode_lzmw_stream(
                  stream, {}, {}, input, {}, encoded).error,
              LzmwStreamCodecError::none);
    StreamHeader parsed{};
    marc::dictionary::internal::LzmwParameters parameters{};
    EXPECT_EQ(decode_lzmw_stream(
                  encoded, {}, {}, {}, {}, parsed, parameters).error,
              LzmwStreamCodecError::none);
}

TEST(LzmwStream, RoundTripsFinalShortBinaryFrame) {
    constexpr std::array raw{
        std::byte{0}, std::byte{0xff}, std::byte{0x7f},
        std::byte{0x80}, std::byte{0x01}};
    const auto stream = config(raw.size(), 4);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 3> encoder{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw, encoder);
    ASSERT_EQ(plan.error, LzmwStreamCodecError::none);
    EXPECT_EQ(plan.frame_count, 2U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_stream(
                  stream, {}, {}, raw, encoder, encoded).error,
              LzmwStreamCodecError::none);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 3> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, raw.size()> output{};
    StreamHeader parsed{};
    marc::dictionary::internal::LzmwParameters parameters{};
    const auto decoded = decode_lzmw_stream(
        encoded, {}, phrases, expansion, output, parsed, parameters);
    EXPECT_EQ(decoded.error, LzmwStreamCodecError::none);
    EXPECT_EQ(decoded.frame_count, 2U);
    EXPECT_EQ(output, raw);
}

TEST(LzmwStream, PlanningAndLaterCorruptionAreAtomic) {
    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> encoder{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw_abab, encoder);
    std::vector<std::byte> short_output(
        plan.serialized_size - 1, std::byte{0x5a});
    EXPECT_EQ(encode_lzmw_stream(
                  stream, {}, {}, raw_abab, encoder, short_output).error,
              LzmwStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    auto encoded = encoded_abab();
    encoded[200] = std::byte{0};
    encoded[201] = std::byte{1};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader parsed{};
    parsed.original_size = 99;
    marc::dictionary::internal::LzmwParameters parameters{};
    parameters.maximum_entries = 7;
    const auto result = decode_lzmw_stream(
        encoded, {}, phrases, expansion, output, parsed, parameters);
    EXPECT_EQ(result.error, LzmwStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_EQ(result.frame_error, LzmwFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(parsed.original_size, 99U);
    EXPECT_EQ(parameters.maximum_entries, 7U);
}

TEST(LzmwStream, PreflightsLaterExpansionWorkspaceBeforePublishing) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3},
        std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}};
    const auto stream = config(raw.size(), 8);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 7> encoder{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw, encoder);
    ASSERT_EQ(plan.error, LzmwStreamCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_stream(
                  stream, {}, {}, raw, encoder, encoded).error,
              LzmwStreamCodecError::none);

    std::array<marc::dictionary::internal::LzmwPhraseEntry, 7> phrases{};
    std::array<std::uint32_t, 5> short_expansion{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader parsed{};
    marc::dictionary::internal::LzmwParameters parameters{};
    const auto result = decode_lzmw_stream(
        encoded, {}, phrases, short_expansion, output, parsed, parameters);
    EXPECT_EQ(result.error, LzmwStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_EQ(result.frame_error, LzmwFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 239;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    std::array<std::uint32_t, 8> full_expansion{};
    const auto limited = decode_lzmw_stream(
        encoded, limits, phrases, full_expansion, output, parsed, parameters);
    EXPECT_EQ(limited.error, LzmwStreamCodecError::frame_error);
    EXPECT_EQ(limited.frame_index, 1U);
    EXPECT_EQ(limited.frame_error, LzmwFrameCodecError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzmwStream, RejectsTruncationTrailingInvalidParametersAndWorkspaces) {
    const auto encoded = encoded_abab();
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    StreamHeader stream{};
    marc::dictionary::internal::LzmwParameters parameters{};
    EXPECT_EQ(decode_lzmw_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, phrases, expansion, output, stream, parameters).error,
              LzmwStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{});
    EXPECT_EQ(decode_lzmw_stream(
                  extended, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzmwStreamCodecError::trailing_stream_bytes);
    auto invalid = encoded;
    std::fill(invalid.begin() + stream_header_size,
              invalid.begin() + stream_header_size + 4, std::byte{});
    EXPECT_EQ(decode_lzmw_stream(
                  invalid, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzmwStreamCodecError::invalid_parameters);
    EXPECT_EQ(decode_lzmw_stream(
                  encoded, {}, {}, expansion, output, stream, parameters).error,
              LzmwStreamCodecError::frame_error);
    output.fill(std::byte{0x5a});
    EXPECT_EQ(decode_lzmw_stream(
                  encoded, {}, phrases, {}, output, stream, parameters).error,
              LzmwStreamCodecError::frame_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    const auto configured = config(raw_abab.size());
    EXPECT_EQ(plan_lzmw_stream(
                  configured, {}, {}, raw_abab, {}).error,
              LzmwStreamCodecError::frame_error);
}

TEST(LzmwStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = config(raw_abab.size() + 1);
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> encoder{};
    EXPECT_EQ(plan_lzmw_stream(
                  stream, {}, {}, raw_abab, encoder).error,
              LzmwStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_abab();
    std::array<std::byte, raw_abab.size() - 1> output{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    marc::dictionary::internal::LzmwParameters parameters{};
    EXPECT_EQ(decode_lzmw_stream(
                  encoded, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzmwStreamCodecError::output_too_small);
}

} // namespace

#include "frame/lzd_stream.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = frame_size;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = size;
    return stream;
}

std::vector<std::byte> encoded_abab() {
    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw_abab, dictionary);
    EXPECT_EQ(plan.error, LzdStreamCodecError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw_abab, dictionary, output).error,
              LzdStreamCodecError::none);
    return output;
}

TEST(LzdStream, EmitsDocumentedMultipleResetFrameVector) {
    constexpr std::array expected{
        std::byte{0x4d}, std::byte{0x41}, std::byte{0x52}, std::byte{0x43},
        std::byte{0x01}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x40}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x05}, std::byte{0}, std::byte{0x01}, std::byte{0},
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
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
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
        std::byte{0x42}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x01}, std::byte{0}, std::byte{0}, std::byte{0},
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
    static_assert(expected.size() == 208);

    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> encoder{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw_abab, encoder);
    ASSERT_EQ(plan.error, LzdStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, expected.size());
    EXPECT_EQ(plan.frame_count, 2U);
    const auto encoded = encoded_abab();
    EXPECT_EQ(std::vector(expected.begin(), expected.end()), encoded);

    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    StreamHeader decoded_stream{};
    marc::dictionary::internal::LzdParameters parameters{};
    const auto result = decode_lzd_stream(
        encoded, {}, phrases, expansion, output, decoded_stream, parameters);
    EXPECT_EQ(result.error, LzdStreamCodecError::none);
    EXPECT_EQ(result.frame_count, 2U);
    EXPECT_EQ(output, raw_abab);
    EXPECT_EQ(parameters.maximum_entries, 65536U);
}

TEST(LzdStream, EmptyInputContainsHeaderAndParameters) {
    const auto stream = config(0);
    const std::array<std::byte, 0> input{};
    const auto plan = plan_lzd_stream(stream, {}, {}, input, {});
    ASSERT_EQ(plan.error, LzdStreamCodecError::none);
    EXPECT_EQ(plan.serialized_size, 80U);
    EXPECT_EQ(plan.frame_count, 0U);
    std::array<std::byte, 80> encoded{};
    ASSERT_EQ(encode_lzd_stream(
                  stream, {}, {}, input, {}, encoded).error,
              LzdStreamCodecError::none);
    StreamHeader parsed{};
    marc::dictionary::internal::LzdParameters parameters{};
    EXPECT_EQ(decode_lzd_stream(
                  encoded, {}, {}, {}, {}, parsed, parameters).error,
              LzdStreamCodecError::none);
}

TEST(LzdStream, RoundTripsFinalShortBinaryFrame) {
    constexpr std::array raw{
        std::byte{0}, std::byte{0xff}, std::byte{0x7f},
        std::byte{0x80}, std::byte{0x01}};
    const auto stream = config(raw.size(), 4);
    std::array<marc::dictionary::internal::LzdEncoderEntry, 2> encoder{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw, encoder);
    ASSERT_EQ(plan.error, LzdStreamCodecError::none);
    EXPECT_EQ(plan.frame_count, 2U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw, encoder, encoded).error,
              LzdStreamCodecError::none);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, raw.size()> output{};
    StreamHeader parsed{};
    marc::dictionary::internal::LzdParameters parameters{};
    const auto decoded = decode_lzd_stream(
        encoded, {}, phrases, expansion, output, parsed, parameters);
    EXPECT_EQ(decoded.error, LzdStreamCodecError::none);
    EXPECT_EQ(decoded.frame_count, 2U);
    EXPECT_EQ(output, raw);
}

TEST(LzdStream, PlanningAndLaterCorruptionAreAtomic) {
    const auto stream = config(raw_abab.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> encoder{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw_abab, encoder);
    std::vector<std::byte> short_output(
        plan.serialized_size - 1, std::byte{0x5a});
    EXPECT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw_abab, encoder, short_output).error,
              LzdStreamCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    auto encoded = encoded_abab();
    encoded[200] = std::byte{0};
    encoded[201] = std::byte{1};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader parsed{};
    parsed.original_size = 99;
    marc::dictionary::internal::LzdParameters parameters{};
    parameters.maximum_entries = 7;
    const auto result = decode_lzd_stream(
        encoded, {}, phrases, expansion, output, parsed, parameters);
    EXPECT_EQ(result.error, LzdStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_EQ(result.frame_error, LzdFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(parsed.original_size, 99U);
    EXPECT_EQ(parameters.maximum_entries, 7U);
}

TEST(LzdStream, PreflightsLaterExpansionWorkspaceBeforePublishing) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3},
        std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}};
    const auto stream = config(raw.size(), 8);
    std::array<marc::dictionary::internal::LzdEncoderEntry, 4> encoder{};
    const auto plan = plan_lzd_stream(stream, {}, {}, raw, encoder);
    ASSERT_EQ(plan.error, LzdStreamCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_stream(
                  stream, {}, {}, raw, encoder, encoded).error,
              LzdStreamCodecError::none);

    std::array<marc::dictionary::internal::LzdPhraseEntry, 4> phrases{};
    std::array<std::uint32_t, 4> expansion{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader parsed{};
    marc::dictionary::internal::LzdParameters parameters{};
    const auto result = decode_lzd_stream(
        encoded, {}, phrases, expansion, output, parsed, parameters);
    EXPECT_EQ(result.error, LzdStreamCodecError::frame_error);
    EXPECT_EQ(result.frame_index, 1U);
    EXPECT_EQ(result.frame_error, LzdFrameCodecError::body_decode_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes = 179;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    std::array<std::uint32_t, 5> full_expansion{};
    const auto limited = decode_lzd_stream(
        encoded, limits, phrases, full_expansion, output, parsed, parameters);
    EXPECT_EQ(limited.error, LzdStreamCodecError::frame_error);
    EXPECT_EQ(limited.frame_index, 1U);
    EXPECT_EQ(limited.frame_error, LzdFrameCodecError::limit_exceeded);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));
}

TEST(LzdStream, RejectsTruncationTrailingInvalidParametersAndWorkspaces) {
    const auto encoded = encoded_abab();
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw_abab.size()> output{};
    StreamHeader stream{};
    marc::dictionary::internal::LzdParameters parameters{};
    EXPECT_EQ(decode_lzd_stream(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  {}, phrases, expansion, output, stream, parameters).error,
              LzdStreamCodecError::truncated_stream);
    auto extended = encoded;
    extended.push_back(std::byte{});
    EXPECT_EQ(decode_lzd_stream(
                  extended, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzdStreamCodecError::trailing_stream_bytes);
    auto invalid = encoded;
    std::fill(invalid.begin() + stream_header_size,
              invalid.begin() + stream_header_size + 4, std::byte{});
    EXPECT_EQ(decode_lzd_stream(
                  invalid, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzdStreamCodecError::invalid_parameters);
    EXPECT_EQ(decode_lzd_stream(
                  encoded, {}, {}, expansion, output, stream, parameters).error,
              LzdStreamCodecError::frame_error);
    output.fill(std::byte{0x5a});
    EXPECT_EQ(decode_lzd_stream(
                  encoded, {}, phrases, {}, output, stream, parameters).error,
              LzdStreamCodecError::frame_error);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](const std::byte value) { return value == std::byte{0x5a}; }));

    const auto configured = config(raw_abab.size());
    EXPECT_EQ(plan_lzd_stream(
                  configured, {}, {}, raw_abab, {}).error,
              LzdStreamCodecError::frame_error);
}

TEST(LzdStream, RejectsInputAndOutputSizeMismatch) {
    auto stream = config(raw_abab.size() + 1);
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> encoder{};
    EXPECT_EQ(plan_lzd_stream(
                  stream, {}, {}, raw_abab, encoder).error,
              LzdStreamCodecError::input_size_mismatch);
    const auto encoded = encoded_abab();
    std::array<std::byte, raw_abab.size() - 1> output{};
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    marc::dictionary::internal::LzdParameters parameters{};
    EXPECT_EQ(decode_lzd_stream(
                  encoded, {}, phrases, expansion, output, stream, parameters)
                  .error,
              LzdStreamCodecError::output_too_small);
}

} // namespace

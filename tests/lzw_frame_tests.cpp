#include "frame/lzw_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::frame;

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(LzwFrame, PlansEncodesValidatesAndDecodesCanonicalFrame) {
    const auto raw = bytes("ABABABA");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzwEncoderEntry, 6>
        encoder_dictionary{};
    const auto plan = plan_lzw_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzwFrameCodecError::none);
    EXPECT_EQ(plan.code_count, 4U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 5U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzw_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzwFrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{0x41});

    std::array<marc::dictionary::internal::LzwPhraseEntry, 3>
        decoder_dictionary{};
    const auto validated = validate_lzw_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary);
    EXPECT_EQ(validated.error, LzwFrameCodecError::none);
    EXPECT_EQ(validated.code_count, 4U);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzw_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary, decoded);
    EXPECT_EQ(result.error, LzwFrameCodecError::none);
    EXPECT_EQ(result.code_count, 4U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzwFrame, EmitsHandCheckableSingleLiteralFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    std::array<std::byte, 58> encoded{};
    ASSERT_EQ(encode_lzw_frame(
                  stream, {}, {}, 0, 0, raw, {}, encoded).error,
              LzwFrameCodecError::none);
    constexpr std::array expected{
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x41}, std::byte{0}};
    EXPECT_EQ(encoded, expected);
}

TEST(LzwFrame, SupportsContextualFinalFrame) {
    const auto raw = bytes("AAA");
    auto stream = config(11);
    stream.frame_size = 8;
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2>
        encoder_dictionary{};
    const auto plan = plan_lzw_frame(
        stream, {}, {}, 1, 8, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzwFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzw_frame(
                  stream, {}, {}, 1, 8, raw, encoder_dictionary, encoded).error,
              LzwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1>
        decoder_dictionary{};
    std::array<std::byte, 3> output{};
    EXPECT_EQ(decode_lzw_frame(
                  stream, {}, {}, 1, 8, encoded, decoder_dictionary, output)
                  .error,
              LzwFrameCodecError::none);
}

TEST(LzwFrame, CapacityMalformedPayloadAndWorkspaceAreAtomic) {
    const auto raw = bytes("AAA");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2>
        encoder_dictionary{};
    const auto plan = plan_lzw_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzw_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1>
        decoder_dictionary{};
    std::array<std::byte, 3> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzw_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary,
        std::span<std::byte>{output}.first(2));
    EXPECT_EQ(result.error, LzwFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = decode_lzw_frame(stream, {}, {}, 0, 0, encoded, {}, output);
    EXPECT_EQ(result.error, LzwFrameCodecError::body_decode_error);
    EXPECT_EQ(result.validation_error,
              marc::dictionary::internal::LzwValidationError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    encoded[frame_header_size] = std::byte{0};
    encoded[frame_header_size + 1] = std::byte{1};
    result = decode_lzw_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary, output);
    EXPECT_EQ(result.error, LzwFrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::LzwFormatError::invalid_first_code);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzwFrame, RejectsExtentHeaderPipelineAndEncoderWorkspaceErrors) {
    const auto raw = bytes("AB");
    auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzwEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzw_frame(
        stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzw_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              LzwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1>
        decode_dictionary{};
    EXPECT_EQ(validate_lzw_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  decode_dictionary).error,
              LzwFrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lzw_frame(
                  stream, {}, {}, 0, 0, encoded, decode_dictionary).error,
              LzwFrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lzw_frame(
                  stream, {}, {}, 0, 0, encoded, decode_dictionary).error,
              LzwFrameCodecError::header_error);

    EXPECT_EQ(plan_lzw_frame(stream, {}, {}, 0, 0, raw, {}).error,
              LzwFrameCodecError::body_encode_error);
    EXPECT_EQ(plan_lzw_frame(stream, {}, {}, 0, 0, {}, dictionary).error,
              LzwFrameCodecError::input_size_mismatch);
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lzw_frame(
                  stream, {}, {}, 0, 0, raw, dictionary).error,
              LzwFrameCodecError::unsupported_pipeline);
}

} // namespace

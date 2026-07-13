#include "frame/lz78_frame.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(Lz78Frame, PlansEncodesValidatesAndDecodesCanonicalFrame) {
    const auto raw = bytes("AABABCABC");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 9>
        encoder_dictionary{};
    const auto plan = plan_lz78_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, Lz78FrameCodecError::none);
    EXPECT_EQ(plan.token_count, 4U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 32U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz78_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              Lz78FrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{0});

    std::array<marc::dictionary::internal::Lz78PhraseEntry, 4>
        decoder_dictionary{};
    const auto validated = validate_lz78_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary);
    EXPECT_EQ(validated.error, Lz78FrameCodecError::none);
    EXPECT_EQ(validated.token_count, 4U);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lz78_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary, decoded);
    EXPECT_EQ(result.error, Lz78FrameCodecError::none);
    EXPECT_EQ(result.token_count, 4U);
    EXPECT_EQ(decoded, raw);
}

TEST(Lz78Frame, EmitsHandCheckableSinglePairFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 1> dictionary{};
    const auto plan = plan_lz78_frame(
        stream, {}, {}, 0, 0, raw, dictionary);
    ASSERT_EQ(plan.serialized_size, 64U);
    std::array<std::byte, 64> encoded{};
    ASSERT_EQ(encode_lz78_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              Lz78FrameCodecError::none);
    constexpr std::array expected{
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0x41}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_EQ(encoded, expected);
}

TEST(Lz78Frame, SupportsContextualFinalFrame) {
    const auto raw = bytes("AAA");
    auto stream = config(11);
    stream.frame_size = 8;
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3>
        encoder_dictionary{};
    const auto plan = plan_lz78_frame(
        stream, {}, {}, 1, 8, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, Lz78FrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz78_frame(
                  stream, {}, {}, 1, 8, raw, encoder_dictionary, encoded).error,
              Lz78FrameCodecError::none);
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decoder_dictionary{};
    std::array<std::byte, 3> output{};
    EXPECT_EQ(decode_lz78_frame(
                  stream, {}, {}, 1, 8, encoded, decoder_dictionary, output)
                  .error,
              Lz78FrameCodecError::none);
}

TEST(Lz78Frame, CapacityMalformedPayloadAndWorkspaceAreAtomic) {
    const auto raw = bytes("AB");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2>
        encoder_dictionary{};
    const auto plan = plan_lz78_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz78_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              Lz78FrameCodecError::none);
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decoder_dictionary{};
    std::array<std::byte, 2> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lz78_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary,
        std::span<std::byte>{output}.first(1));
    EXPECT_EQ(result.error, Lz78FrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    encoded[frame_header_size
            + marc::dictionary::internal::lz78_token_size + 4] = std::byte{2};
    result = decode_lz78_frame(
        stream, {}, {}, 0, 0, encoded, decoder_dictionary, output);
    EXPECT_EQ(result.error, Lz78FrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::Lz78FormatError::invalid_phrase_index);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = decode_lz78_frame(stream, {}, {}, 0, 0, encoded, {}, output);
    EXPECT_EQ(result.error, Lz78FrameCodecError::body_decode_error);
    EXPECT_EQ(result.validation_error,
              marc::dictionary::internal::Lz78ValidationError::workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(Lz78Frame, RejectsExtentHeaderPipelineAndEncoderWorkspaceErrors) {
    const auto raw = bytes("AB");
    auto stream = config(raw.size());
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 2> dictionary{};
    const auto plan = plan_lz78_frame(
        stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz78_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              Lz78FrameCodecError::none);
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2>
        decode_dictionary{};
    EXPECT_EQ(validate_lz78_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  decode_dictionary).error,
              Lz78FrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lz78_frame(
                  stream, {}, {}, 0, 0, encoded, decode_dictionary).error,
              Lz78FrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lz78_frame(
                  stream, {}, {}, 0, 0, encoded, decode_dictionary).error,
              Lz78FrameCodecError::header_error);

    EXPECT_EQ(plan_lz78_frame(stream, {}, {}, 0, 0, raw, {}).error,
              Lz78FrameCodecError::body_encode_error);
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lz78_frame(
                  stream, {}, {}, 0, 0, raw, dictionary).error,
              Lz78FrameCodecError::unsupported_pipeline);
}

} // namespace

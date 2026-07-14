#include "frame/lzd_frame.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzd;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzd_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(LzdFrame, PlansEncodesValidatesAndDecodesCanonicalFrame) {
    const auto raw = bytes("abbaababaaba");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 6>
        encoder_dictionary{};
    const auto plan = plan_lzd_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzdFrameCodecError::none);
    EXPECT_EQ(plan.token_count, 5U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 40U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzdFrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{'a'});

    std::array<marc::dictionary::internal::LzdPhraseEntry, 5> phrases{};
    const auto validated = validate_lzd_frame(
        stream, {}, {}, 0, 0, encoded, phrases);
    EXPECT_EQ(validated.error, LzdFrameCodecError::none);
    EXPECT_EQ(validated.token_count, 5U);
    std::array<std::uint32_t, 6> expansion{};
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzd_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion, decoded);
    EXPECT_EQ(result.error, LzdFrameCodecError::none);
    EXPECT_EQ(result.token_count, 5U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzdFrame, EmitsDocumentedSingleByteFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    const auto plan = plan_lzd_frame(stream, {}, {}, 0, 0, raw, {});
    ASSERT_EQ(plan.error, LzdFrameCodecError::none);
    ASSERT_EQ(plan.serialized_size, 64U);
    std::array<std::byte, 64> encoded{};
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 0, 0, raw, {}, encoded).error,
              LzdFrameCodecError::none);
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
        std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
    EXPECT_EQ(encoded, expected);
}

TEST(LzdFrame, SupportsContextualFinalFrame) {
    const auto raw = bytes("ABA");
    auto stream = config(11);
    stream.frame_size = 8;
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1>
        encoder_dictionary{};
    const auto plan = plan_lzd_frame(
        stream, {}, {}, 1, 8, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzdFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 1, 8, raw, encoder_dictionary, encoded).error,
              LzdFrameCodecError::none);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 3> output{};
    EXPECT_EQ(decode_lzd_frame(
                  stream, {}, {}, 1, 8, encoded, phrases, expansion, output)
                  .error,
              LzdFrameCodecError::none);
    EXPECT_EQ(std::vector(output.begin(), output.end()), raw);
}

TEST(LzdFrame, CapacityMalformedPayloadAndWorkspacesAreAtomic) {
    const auto raw = bytes("ABAB");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 2>
        encoder_dictionary{};
    const auto plan = plan_lzd_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    std::vector<std::byte> short_encoded(
        plan.serialized_size - 1, std::byte{0xcc});
    auto short_result = encode_lzd_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary, short_encoded);
    EXPECT_EQ(short_result.error, LzdFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_encoded, [](const std::byte value) {
            return value == std::byte{0xcc};
        }));
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzdFrameCodecError::none);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzd_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion,
        std::span<std::byte>{output}.first(3));
    EXPECT_EQ(result.error, LzdFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    encoded[frame_header_size
            + marc::dictionary::internal::lzd_token_size] = std::byte{0x01};
    result = decode_lzd_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion, output);
    EXPECT_EQ(result.error, LzdFrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::LzdFormatError::invalid_phrase_reference);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = decode_lzd_frame(
        stream, {}, {}, 0, 0, encoded, {}, expansion, output);
    EXPECT_EQ(result.error, LzdFrameCodecError::body_decode_error);
    EXPECT_EQ(result.validation_error,
              marc::dictionary::internal::LzdValidationError::workspace_too_small);
    encoded[frame_header_size
            + marc::dictionary::internal::lzd_token_size] = std::byte{};
    result = decode_lzd_frame(
        stream, {}, {}, 0, 0, encoded, phrases, {}, output);
    EXPECT_EQ(result.error, LzdFrameCodecError::body_decode_error);
    EXPECT_EQ(result.decode_error,
              marc::dictionary::internal::LzdDecodeError::expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzdFrame, RejectsExtentHeaderPipelineAndEncoderWorkspaceErrors) {
    const auto raw = bytes("AB");
    auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzd_frame(
        stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              LzdFrameCodecError::none);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    EXPECT_EQ(validate_lzd_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  phrases).error,
              LzdFrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lzd_frame(
                  stream, {}, {}, 0, 0, encoded, phrases).error,
              LzdFrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lzd_frame(
                  stream, {}, {}, 0, 0, encoded, phrases).error,
              LzdFrameCodecError::header_error);

    EXPECT_EQ(plan_lzd_frame(stream, {}, {}, 0, 0, raw, {}).error,
              LzdFrameCodecError::body_encode_error);
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lzd_frame(
                  stream, {}, {}, 0, 0, raw, dictionary).error,
              LzdFrameCodecError::unsupported_pipeline);
}

TEST(LzdFrame, EnforcesWholeFrameAggregateLimits) {
    const auto raw = bytes("AB");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzdEncoderEntry, 1> dictionary{};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        raw.size() + frame_header_size
        + marc::dictionary::internal::lzd_token_size
        + sizeof(marc::dictionary::internal::LzdEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(plan_lzd_frame(
                  stream, {}, limits, 0, 0, raw, dictionary).error,
              LzdFrameCodecError::limit_exceeded);

    const auto plan = plan_lzd_frame(stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzd_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              LzdFrameCodecError::none);
    std::array<marc::dictionary::internal::LzdPhraseEntry, 1> phrases{};
    limits = {};
    limits.max_internal_buffered_bytes =
        encoded.size()
        + sizeof(marc::dictionary::internal::LzdPhraseEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(validate_lzd_frame(
                  stream, {}, limits, 0, 0, encoded, phrases).error,
              LzdFrameCodecError::limit_exceeded);

    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 2> output{};
    limits.max_internal_buffered_bytes =
        encoded.size() + output.size()
        + sizeof(marc::dictionary::internal::LzdPhraseEntry)
        + expansion.size() * sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(decode_lzd_frame(
                  stream, {}, limits, 0, 0, encoded, phrases, expansion,
                  output).error,
              LzdFrameCodecError::limit_exceeded);
}

} // namespace

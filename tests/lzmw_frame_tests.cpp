#include "frame/lzmw_frame.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(LzmwFrame, PlansEncodesValidatesAndDecodesPublishedFrame) {
    const auto raw = bytes("abbaababaaba");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 11>
        encoder_dictionary{};
    const auto plan = plan_lzmw_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzmwFrameCodecError::none);
    EXPECT_EQ(plan.token_count, 8U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 32U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzmwFrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{'a'});

    std::array<marc::dictionary::internal::LzmwPhraseEntry, 7> phrases{};
    const auto validated = validate_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, phrases);
    EXPECT_EQ(validated.error, LzmwFrameCodecError::none);
    EXPECT_EQ(validated.token_count, 8U);
    std::array<std::uint32_t, 8> expansion{};
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion, decoded);
    EXPECT_EQ(result.error, LzmwFrameCodecError::none);
    EXPECT_EQ(result.token_count, 8U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzmwFrame, EmitsDocumentedSingleByteFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    const auto plan = plan_lzmw_frame(stream, {}, {}, 0, 0, raw, {});
    ASSERT_EQ(plan.error, LzmwFrameCodecError::none);
    ASSERT_EQ(plan.serialized_size, 60U);
    std::array<std::byte, 60> encoded{};
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, {}, encoded).error,
              LzmwFrameCodecError::none);
    constexpr std::array expected{
        std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x41}, std::byte{0}, std::byte{0}, std::byte{0}};
    EXPECT_EQ(encoded, expected);
}

TEST(LzmwFrame, SupportsContextualFinalFrame) {
    const auto raw = bytes("ABA");
    auto stream = config(11);
    stream.frame_size = 8;
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 2>
        encoder_dictionary{};
    const auto plan = plan_lzmw_frame(
        stream, {}, {}, 1, 8, raw, encoder_dictionary);
    ASSERT_EQ(plan.error, LzmwFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 1, 8, raw, encoder_dictionary, encoded).error,
              LzmwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 3> output{};
    EXPECT_EQ(decode_lzmw_frame(
                  stream, {}, {}, 1, 8, encoded, phrases, expansion, output)
                  .error,
              LzmwFrameCodecError::none);
    EXPECT_EQ(std::vector(output.begin(), output.end()), raw);
}

TEST(LzmwFrame, CapacityMalformedPayloadAndWorkspacesAreAtomic) {
    const auto raw = bytes("ABAB");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 3>
        encoder_dictionary{};
    const auto plan = plan_lzmw_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary);
    std::vector<std::byte> short_encoded(
        plan.serialized_size - 1, std::byte{0xcc});
    auto short_result = encode_lzmw_frame(
        stream, {}, {}, 0, 0, raw, encoder_dictionary, short_encoded);
    EXPECT_EQ(short_result.error, LzmwFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_encoded, [](const std::byte value) {
            return value == std::byte{0xcc};
        }));
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, encoder_dictionary, encoded).error,
              LzmwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 2> phrases{};
    std::array<std::uint32_t, 3> expansion{};
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion,
        std::span<std::byte>{output}.first(3));
    EXPECT_EQ(result.error, LzmwFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    encoded[frame_header_size + 2 *
            marc::dictionary::internal::lzmw_token_size] = std::byte{0x01};
    result = decode_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, phrases, expansion, output);
    EXPECT_EQ(result.error, LzmwFrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::LzmwFormatError::invalid_phrase_reference);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    result = decode_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, {}, expansion, output);
    EXPECT_EQ(result.error, LzmwFrameCodecError::body_decode_error);
    EXPECT_EQ(result.validation_error,
              marc::dictionary::internal::LzmwValidationError::workspace_too_small);
    encoded[frame_header_size + 2 *
            marc::dictionary::internal::lzmw_token_size] = std::byte{};
    result = decode_lzmw_frame(
        stream, {}, {}, 0, 0, encoded, phrases, {}, output);
    EXPECT_EQ(result.error, LzmwFrameCodecError::body_decode_error);
    EXPECT_EQ(result.decode_error,
              marc::dictionary::internal::LzmwDecodeError::expansion_workspace_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzmwFrame, RejectsExtentHeaderPipelineAndEncoderWorkspaceErrors) {
    const auto raw = bytes("AB");
    auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzmw_frame(
        stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              LzmwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    EXPECT_EQ(validate_lzmw_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  phrases).error,
              LzmwFrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lzmw_frame(
                  stream, {}, {}, 0, 0, encoded, phrases).error,
              LzmwFrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lzmw_frame(
                  stream, {}, {}, 0, 0, encoded, phrases).error,
              LzmwFrameCodecError::header_error);

    EXPECT_EQ(plan_lzmw_frame(stream, {}, {}, 0, 0, raw, {}).error,
              LzmwFrameCodecError::body_encode_error);
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, dictionary).error,
              LzmwFrameCodecError::unsupported_pipeline);
}

TEST(LzmwFrame, EnforcesWholeFrameAggregateLimits) {
    const auto raw = bytes("AB");
    const auto stream = config(raw.size());
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> dictionary{};
    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        raw.size() + frame_header_size
        + 2 * marc::dictionary::internal::lzmw_token_size
        + sizeof(marc::dictionary::internal::LzmwEncoderEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(plan_lzmw_frame(
                  stream, {}, limits, 0, 0, raw, dictionary).error,
              LzmwFrameCodecError::limit_exceeded);

    const auto plan = plan_lzmw_frame(stream, {}, {}, 0, 0, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzmw_frame(
                  stream, {}, {}, 0, 0, raw, dictionary, encoded).error,
              LzmwFrameCodecError::none);
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    limits = {};
    limits.max_internal_buffered_bytes =
        encoded.size()
        + sizeof(marc::dictionary::internal::LzmwPhraseEntry) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(validate_lzmw_frame(
                  stream, {}, limits, 0, 0, encoded, phrases).error,
              LzmwFrameCodecError::limit_exceeded);

    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, 2> output{};
    limits.max_internal_buffered_bytes =
        encoded.size() + output.size()
        + sizeof(marc::dictionary::internal::LzmwPhraseEntry)
        + expansion.size() * sizeof(std::uint32_t) - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    EXPECT_EQ(decode_lzmw_frame(
                  stream, {}, limits, 0, 0, encoded, phrases, expansion,
                  output).error,
              LzmwFrameCodecError::limit_exceeded);
}

} // namespace

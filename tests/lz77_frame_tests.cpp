#include "frame/lz77_frame.hpp"

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
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(Lz77Frame, PlansEncodesAndDecodesCanonicalFrame) {
    const auto raw = bytes("ABCABCXAAAA");
    const auto stream = config(raw.size());
    const auto plan = plan_lz77_frame(stream, {}, {}, 0, 0, raw);
    ASSERT_EQ(plan.error, Lz77FrameCodecError::none);
    EXPECT_EQ(plan.token_count, 6U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 6 * 16);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz77_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              Lz77FrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{0});
    EXPECT_EQ(validate_lz77_frame(stream, {}, {}, 0, 0, encoded).error,
              Lz77FrameCodecError::none);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lz77_frame(stream, {}, {}, 0, 0, encoded,
                                          decoded);
    EXPECT_EQ(result.error, Lz77FrameCodecError::none);
    EXPECT_EQ(decoded, raw);
}

TEST(Lz77Frame, EmitsHandCheckableSingleLiteralFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    const auto plan = plan_lz77_frame(stream, {}, {}, 0, 0, raw);
    ASSERT_EQ(plan.serialized_size, 72U);
    std::array<std::byte, 72> encoded{};
    ASSERT_EQ(encode_lz77_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              Lz77FrameCodecError::none);
    constexpr std::array expected{
        std::byte{0x4D}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
        std::byte{0x38}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x10}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
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

TEST(Lz77Frame, SupportsContextualFinalFrame) {
    const auto raw = bytes("AAAA");
    auto stream = config(12);
    stream.frame_size = 8;
    const auto plan = plan_lz77_frame(stream, {}, {}, 1, 8, raw);
    ASSERT_EQ(plan.error, Lz77FrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz77_frame(stream, {}, {}, 1, 8, raw, encoded).error,
              Lz77FrameCodecError::none);
    std::array<std::byte, 4> output{};
    EXPECT_EQ(decode_lz77_frame(stream, {}, {}, 1, 8, encoded, output).error,
              Lz77FrameCodecError::none);
}

TEST(Lz77Frame, CapacityAndMalformedPayloadAreAtomic) {
    const auto raw = bytes("AAAA");
    const auto stream = config(raw.size());
    const auto plan = plan_lz77_frame(stream, {}, {}, 0, 0, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz77_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              Lz77FrameCodecError::none);
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xCC});
    auto result = decode_lz77_frame(
        stream, {}, {}, 0, 0, encoded,
        std::span<std::byte>{output}.first(3));
    EXPECT_EQ(result.error, Lz77FrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](std::byte value) { return value == std::byte{0xCC}; }));

    encoded[frame_header_size + 16 + 4] = std::byte{2};
    result = decode_lz77_frame(stream, {}, {}, 0, 0, encoded, output);
    EXPECT_EQ(result.error, Lz77FrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::Lz77FormatError::invalid_distance);
    EXPECT_TRUE(std::ranges::all_of(output,
        [](std::byte value) { return value == std::byte{0xCC}; }));
}

TEST(Lz77Frame, RejectsExtentHeaderAndPipelineErrors) {
    const auto raw = bytes("AAAA");
    auto stream = config(raw.size());
    const auto plan = plan_lz77_frame(stream, {}, {}, 0, 0, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lz77_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              Lz77FrameCodecError::none);
    EXPECT_EQ(validate_lz77_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1))
                  .error,
              Lz77FrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lz77_frame(stream, {}, {}, 0, 0, encoded).error,
              Lz77FrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lz77_frame(stream, {}, {}, 0, 0, encoded).error,
              Lz77FrameCodecError::header_error);

    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lz77_frame(stream, {}, {}, 0, 0, raw).error,
              Lz77FrameCodecError::unsupported_pipeline);
}

} // namespace

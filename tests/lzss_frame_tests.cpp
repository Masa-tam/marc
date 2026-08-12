#include "frame/lzss_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
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
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

TEST(LzssFrame, PlansEncodesValidatesAndDecodesCanonicalFrame) {
    const auto raw = bytes("ABCABCABCXAAAAAA");
    const auto stream = config(raw.size());
    const auto plan = plan_lzss_frame(stream, {}, {}, 0, 0, raw);
    ASSERT_EQ(plan.error, LzssFrameCodecError::none);
    EXPECT_EQ(plan.token_count, 7U);
    EXPECT_EQ(plan.serialized_size, frame_header_size + 28U);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              LzssFrameCodecError::none);
    EXPECT_EQ(encoded[0], std::byte{'M'});
    EXPECT_EQ(encoded[frame_header_size], std::byte{0});
    const auto validated = validate_lzss_frame(
        stream, {}, {}, 0, 0, encoded);
    EXPECT_EQ(validated.error, LzssFrameCodecError::none);
    EXPECT_EQ(validated.token_count, 7U);
    std::vector<std::byte> decoded(raw.size());
    const auto result = decode_lzss_frame(stream, {}, {}, 0, 0, encoded,
                                          decoded);
    EXPECT_EQ(result.error, LzssFrameCodecError::none);
    EXPECT_EQ(result.token_count, 7U);
    EXPECT_EQ(decoded, raw);
}

TEST(LzssFrame, HashChainMatchesExhaustiveAndFailsAtomically) {
    const auto raw = bytes("ABCDE1ABCDE2ABCDE3");
    const auto stream = config(raw.size());
    const auto reference_plan = plan_lzss_frame(stream, {}, {}, 0, 0, raw);
    ASSERT_EQ(reference_plan.error, LzssFrameCodecError::none);
    std::vector<std::byte> reference(reference_plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 0, 0, raw, reference).error,
              LzssFrameCodecError::none);

    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(raw.size(), {}, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    ASSERT_GT(requirements.workspace_size, 0U);
    std::vector<std::max_align_t> finder_backing(
        (requirements.workspace_size + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t));
    auto finder = std::as_writable_bytes(std::span{finder_backing}).first(
        requirements.workspace_size);
    const auto plan = plan_lzss_frame_hash_chain(
        stream, {}, {}, 0, 0, raw, finder);
    ASSERT_EQ(plan.error, LzssFrameCodecError::none);
    EXPECT_EQ(plan.serialized_size, reference_plan.serialized_size);
    EXPECT_EQ(plan.output_size, reference_plan.output_size);
    EXPECT_EQ(plan.token_count, reference_plan.token_count);

    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame_hash_chain(
                  stream, {}, {}, 0, 0, raw, finder, encoded).error,
              LzssFrameCodecError::none);
    EXPECT_EQ(encoded, reference);
    std::vector<std::byte> decoded(raw.size());
    EXPECT_EQ(decode_lzss_frame(
                  stream, {}, {}, 0, 0, encoded, decoded).error,
              LzssFrameCodecError::none);
    EXPECT_EQ(decoded, raw);

    std::ranges::fill(encoded, std::byte{0xcc});
    auto failed = encode_lzss_frame_hash_chain(
        stream, {}, {}, 0, 0, raw,
        finder.first(finder.size() - 1), encoded);
    EXPECT_EQ(failed.error, LzssFrameCodecError::body_encode_error);
    EXPECT_EQ(failed.encode_error,
              marc::dictionary::internal::LzssEncodeError::
                  match_finder_error);
    EXPECT_TRUE(std::ranges::all_of(encoded, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    failed = encode_lzss_frame_hash_chain(
        stream, {}, {}, 0, 0, raw, finder,
        finder.first(encoded.size()));
    EXPECT_EQ(failed.error, LzssFrameCodecError::body_encode_error);
    EXPECT_EQ(failed.encode_error,
              marc::dictionary::internal::LzssEncodeError::
                  overlapping_buffers);

    std::vector<std::byte> shared(
        std::max(raw.size(), encoded.size()), std::byte{0xcc});
    std::ranges::copy(raw, shared.begin());
    failed = encode_lzss_frame_hash_chain(
        stream, {}, {}, 0, 0,
        std::span<const std::byte>{shared}.first(raw.size()), finder,
        std::span<std::byte>{shared}.first(encoded.size()));
    EXPECT_EQ(failed.encode_error,
              marc::dictionary::internal::LzssEncodeError::
                  overlapping_buffers);

    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = raw.size();
    limits.max_internal_buffered_bytes = raw.size() + finder.size()
        + plan.serialized_size - 1;
    failed = plan_lzss_frame_hash_chain(
        stream, {}, limits, 0, 0, raw, finder);
    EXPECT_EQ(failed.error, LzssFrameCodecError::body_encode_error);
    EXPECT_EQ(failed.encode_error,
              marc::dictionary::internal::LzssEncodeError::
                  serialized_limit_exceeded);
}

TEST(LzssFrame, EmitsHandCheckableSingleLiteralFrame) {
    const auto raw = bytes("A");
    const auto stream = config(raw.size());
    const auto plan = plan_lzss_frame(stream, {}, {}, 0, 0, raw);
    ASSERT_EQ(plan.serialized_size, 58U);
    std::array<std::byte, 58> encoded{};
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              LzssFrameCodecError::none);
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
        std::byte{0x00}, std::byte{0x41}};
    EXPECT_EQ(encoded, expected);
}

TEST(LzssFrame, SupportsContextualFinalFrame) {
    const auto raw = bytes("AAAAAA");
    auto stream = config(14);
    stream.frame_size = 8;
    const auto plan = plan_lzss_frame(stream, {}, {}, 1, 8, raw);
    ASSERT_EQ(plan.error, LzssFrameCodecError::none);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 1, 8, raw, encoded).error,
              LzssFrameCodecError::none);
    std::array<std::byte, 6> output{};
    EXPECT_EQ(decode_lzss_frame(stream, {}, {}, 1, 8, encoded, output).error,
              LzssFrameCodecError::none);
}

TEST(LzssFrame, CapacityAndMalformedPayloadAreAtomic) {
    const auto raw = bytes("AAAAAA");
    const auto stream = config(raw.size());
    const auto plan = plan_lzss_frame(stream, {}, {}, 0, 0, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              LzssFrameCodecError::none);
    std::array<std::byte, 6> output{};
    output.fill(std::byte{0xcc});
    auto result = decode_lzss_frame(
        stream, {}, {}, 0, 0, encoded,
        std::span<std::byte>{output}.first(5));
    EXPECT_EQ(result.error, LzssFrameCodecError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));

    encoded[frame_header_size + 3] = std::byte{2};
    result = decode_lzss_frame(stream, {}, {}, 0, 0, encoded, output);
    EXPECT_EQ(result.error, LzssFrameCodecError::body_decode_error);
    EXPECT_EQ(result.format_error,
              marc::dictionary::internal::LzssFormatError::invalid_distance);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssFrame, RejectsExtentHeaderAndPipelineErrors) {
    const auto raw = bytes("AAAAAA");
    auto stream = config(raw.size());
    const auto plan = plan_lzss_frame(stream, {}, {}, 0, 0, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(encode_lzss_frame(stream, {}, {}, 0, 0, raw, encoded).error,
              LzssFrameCodecError::none);
    EXPECT_EQ(validate_lzss_frame(
                  stream, {}, {}, 0, 0,
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1))
                  .error,
              LzssFrameCodecError::truncated_frame);
    encoded.push_back(std::byte{});
    EXPECT_EQ(validate_lzss_frame(stream, {}, {}, 0, 0, encoded).error,
              LzssFrameCodecError::trailing_frame_bytes);
    encoded.pop_back();
    encoded[8] = std::byte{1};
    EXPECT_EQ(validate_lzss_frame(stream, {}, {}, 0, 0, encoded).error,
              LzssFrameCodecError::header_error);

    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    EXPECT_EQ(plan_lzss_frame(stream, {}, {}, 0, 0, raw).error,
              LzssFrameCodecError::unsupported_pipeline);
}

} // namespace

#include "frame/lzss_contextual_blocked_huffman_frame_streaming_encoder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::core::ErrorCode;
using marc::core::ProcessFlags;
using marc::core::StreamStatus;
using marc::dictionary::internal::LzssTypedToken;

[[nodiscard]] LzssContextualBlockedHuffmanStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    LzssContextualBlockedHuffmanStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> literal_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const std::uint64_t sequence) {
    std::vector<std::byte> bytes(88);
    const LzssContextualBlockedHuffmanFrameHeader header{
        0, sequence, 1, 1, 2, 2, 0, 24, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, sequence, sequence},
                  std::span<std::byte, 64>{bytes.data(), 64}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{15}, std::byte{3}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{65}, std::byte{0}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    const auto stream = stream_config(1, 2);
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        header{};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  stream, {}, header),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    const auto first = literal_frame(stream, 0);
    const auto second = literal_frame(stream, 1);
    std::vector<std::byte> expected(header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     HashChainMatchesReferenceAndEnforcesFinderBoundaries) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    const auto stream = stream_config(input.size(), input.size());
    std::array<LzssTypedToken, input.size()> reference_tokens{};
    const auto plan = plan_lzss_contextual_blocked_huffman_frame(
        stream, {}, 0, 0, input, reference_tokens);
    ASSERT_EQ(plan.error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    std::vector<std::byte> reference_frame(plan.serialized_size);
    ASSERT_EQ(encode_lzss_contextual_blocked_huffman_frame(
                  stream, {}, 0, 0, input, reference_tokens,
                  reference_frame).error,
              LzssContextualBlockedHuffmanFrameEncodeError::none);
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        header{};
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  stream, {}, header),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    std::vector<std::byte> expected(header.begin(), header.end());
    expected.insert(
        expected.end(), reference_frame.begin(), reference_frame.end());

    const auto requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(input.size(), stream.dictionary, {});
    ASSERT_EQ(requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    std::vector<std::max_align_t> finder_backing(
        (requirements.workspace_size + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t));
    auto finder = std::as_writable_bytes(std::span{finder_backing}).first(
        requirements.workspace_size);
    std::array<std::byte, input.size()> raw{};
    std::array<LzssTypedToken, input.size()> tokens{};
    std::vector<std::byte> frame(plan.serialized_size);
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream, {}, raw, tokens, finder, frame};
    std::vector<std::byte> actual(expected.size());
    const auto result = encoder.process(input, actual, end_flag());
    ASSERT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, input.size());
    EXPECT_EQ(result.output_produced, expected.size());
    EXPECT_EQ(actual, expected);

    LzssContextualBlockedHuffmanFrameStreamingEncoder short_finder{
        stream, {}, raw, tokens, finder.first(finder.size() - 1), frame};
    std::ranges::fill(actual, std::byte{0xcc});
    const auto failed = short_finder.process(input, actual, end_flag());
    EXPECT_EQ(failed.status, StreamStatus::error);
    EXPECT_EQ(failed.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(failed.output_produced, header.size());

    LzssContextualBlockedHuffmanFrameStreamingEncoder output_alias{
        stream, {}, raw, tokens, finder, frame};
    EXPECT_EQ(output_alias.process({}, finder, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     MatchesOneByteOracle) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    const auto expected = two_frame_stream();
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 88> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, frame};

    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? end_flag()
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              StreamStatus::end_of_stream);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     EmitsFullFrameBeforeEndAndFlushKeepsPartialOpen) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 256> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, frame};
    std::vector<std::byte> output(1024);

    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lzss_contextual_blocked_huffman_stream_header_size);
    EXPECT_EQ(first.status, StreamStatus::progress);

    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(1, 1),
        std::span<std::byte>{output}.subspan(first.output_produced), 0);
    EXPECT_EQ(second.input_consumed, 1U);
    EXPECT_GT(second.output_produced, 0U);
    EXPECT_EQ(second.status, StreamStatus::progress);

    const auto third = encoder.process(
        std::span<const std::byte>{input}.last(1),
        std::span<std::byte>{output}.subspan(
            first.output_produced + second.output_produced),
        end_flag());
    EXPECT_EQ(third.input_consumed, 1U);
    EXPECT_EQ(third.status, StreamStatus::end_of_stream);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     RetainsEndInputAcrossFinalAndEmptyDrain) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 88> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder encoder{
        stream_config(1, 1), {}, raw, tokens, frame};
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        header{};
    const auto header_result = encoder.process({}, header, 0);
    ASSERT_EQ(header_result.status, StreamStatus::progress);
    auto result = encoder.process(input, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 1U);
    std::array<std::byte, 88> encoded_frame{};
    result = encoder.process({}, encoded_frame, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded_frame.size());

    LzssContextualBlockedHuffmanFrameStreamingEncoder empty{
        stream_config(1, 0), {}, {}, {}, {}};
    result = empty.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    result = empty.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
}

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     ReportsCapacityLimitAndInputProtocolFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 88> frame{};
    std::vector<std::byte> output(512);

    LzssContextualBlockedHuffmanFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualBlockedHuffmanFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens,
        std::span<std::byte>{frame}.first(frame.size() - 1)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    constexpr std::array limited_input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    std::array<std::byte, limited_input.size()> limited_raw{};
    std::array<LzssTypedToken, limited_input.size()> limited_tokens{};
    std::array<std::byte, 512> limited_frame{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        lzss_contextual_blocked_huffman_stream_header_size;
    LzssContextualBlockedHuffmanFrameStreamingEncoder limited{
        stream_config(limited_input.size(), limited_input.size()), limits,
        limited_raw, limited_tokens, limited_frame};
    result = limited.process(limited_input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> premature_raw{};
    std::array<LzssTypedToken, 2> premature_tokens{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder premature{
        stream_config(2, 2), {}, premature_raw, premature_tokens, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualBlockedHuffmanFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 1> shared{};
    auto shared_bytes = std::as_writable_bytes(std::span{shared});
    std::array<std::byte, 88> frame{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, shared_bytes.first(1), shared, frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 88> raw_frame_shared{};
    std::array<LzssTypedToken, 1> tokens{};
    LzssContextualBlockedHuffmanFrameStreamingEncoder raw_frame_overlap{
        stream_config(1, 1), {},
        std::span<std::byte>{raw_frame_shared}.first(1), tokens,
        raw_frame_shared};
    EXPECT_EQ(raw_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    LzssContextualBlockedHuffmanFrameStreamingEncoder token_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, token_bytes};
    EXPECT_EQ(token_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualBlockedHuffmanFrameStreamingEncoder raw_output_alias{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(raw_output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(raw_output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualBlockedHuffmanFrameStreamingEncoder token_output_alias{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(token_output_alias.process({}, token_bytes, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualBlockedHuffmanFrameStreamingEncoder frame_output_alias{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(frame_output_alias.process({}, frame, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualBlockedHuffmanFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, frame};
    auto result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              ErrorCode::unsupported);

    LzssContextualBlockedHuffmanFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, frame};
    result = reset.process(
        {}, {}, marc::core::flag_value(ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              ErrorCode::unsupported);
}

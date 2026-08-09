#include "frame/lzss_contextual_rans_compact_frame_streaming_encoder.hpp"

#include "frame/lzss_contextual_rans_compact_frame_encoder.hpp"
#include "frame/lzss_contextual_rans_compact_frame_streaming_decoder.hpp"

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
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

[[nodiscard]] LzssContextualRansStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    LzssContextualRansStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

[[nodiscard]] std::vector<std::byte> encode_one_byte_chunks(
    LzssContextualRansCompactFrameStreamingEncoder& encoder,
    const std::span<const std::byte> input) {
    std::vector<std::byte> output;
    std::size_t input_offset{};
    std::array<std::byte, 1> byte{};
    StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = input.subspan(input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? end_flag()
            : 0U;
        const auto result = encoder.process(chunk, byte, flags);
        EXPECT_TRUE(marc::core::is_valid(result, chunk.size(), byte.size()));
        EXPECT_NE(result.status, StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) output.push_back(byte[0]);
        status = result.status;
    } while (status != StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    return output;
}

[[nodiscard]] std::vector<std::byte> two_frame_oracle() {
    constexpr std::array raw{std::byte{'A'}};
    const auto stream = stream_config(1, 2);
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    EXPECT_EQ(serialize_lzss_contextual_rans_compact_stream_header(
                  stream, {}, header),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(header[18], std::byte{0x03});
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> first{};
    std::array<std::byte, 98> second{};
    EXPECT_EQ(encode_lzss_contextual_rans_compact_frame(
                  stream, {}, 0, 0, raw, tokens, first).error,
              LzssContextualRansFrameEncodeError::none);
    EXPECT_EQ(encode_lzss_contextual_rans_compact_frame(
                  stream, {}, 1, 1, raw, tokens, second).error,
              LzssContextualRansFrameEncodeError::none);
    std::vector<std::byte> expected;
    expected.insert(expected.end(), header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

} // namespace

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     MatchesCompactOracleWithOneByteChunks) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    LzssContextualRansCompactFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);
    EXPECT_EQ(encoded, two_frame_oracle());
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              StreamStatus::end_of_stream);
}

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     CompactStreamingDecoderRecoversOutput) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    LzssContextualRansCompactFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);

    std::array<std::byte, 98> serialized{};
    std::vector<RansDecodeEntry> tables(
        contextual_rans_decode_table_entries);
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decode_raw{};
    LzssContextualRansCompactFrameStreamingDecoder decoder{
        {}, serialized, tables, decode_tokens, decode_raw};
    std::array<std::byte, input.size()> decoded{};
    const auto result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, encoded.size());
    EXPECT_EQ(result.output_produced, decoded.size());
    EXPECT_EQ(decoded, input);
}

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     FlushKeepsPartialFrameOpenAndEndInputSurvivesDrain) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 256> frame{};
    LzssContextualRansCompactFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, frame};
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    auto result = encoder.process(
        std::span<const std::byte>{input}.first(1), header,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(result.input_consumed, 1U);
    EXPECT_EQ(result.output_produced, header.size());
    EXPECT_EQ(result.status, StreamStatus::progress);

    result = encoder.process(
        std::span<const std::byte>{input}.subspan(1), {}, end_flag());
    EXPECT_EQ(result.input_consumed, 1U);
    EXPECT_EQ(result.status, StreamStatus::need_output);
    std::vector<std::byte> output(512);
    while (result.status != StreamStatus::end_of_stream) {
        result = encoder.process(
            std::span<const std::byte>{input}.last(1), output, end_flag());
        ASSERT_NE(result.status, StreamStatus::error);
    }
}

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     EmptyInputEndsAfterHeaderDrain) {
    LzssContextualRansCompactFrameStreamingEncoder encoder{
        stream_config(1, 0), {}, {}, {}, {}};
    auto result = encoder.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    result = encoder.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
    EXPECT_EQ(header[18], std::byte{0x03});
}

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     ReportsCapacityLimitAndInputFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    std::vector<std::byte> output(256);

    LzssContextualRansCompactFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansCompactFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens,
        std::span<std::byte>{frame}.first(frame.size() - 1)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = 112;
    constexpr std::array limit_input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'F'}, std::byte{'G'}, std::byte{'H'}};
    std::array<std::byte, limit_input.size()> limit_raw{};
    std::array<LzssTypedToken, limit_input.size()> limit_tokens{};
    LzssContextualRansCompactFrameStreamingEncoder limited{
        stream_config(limit_input.size(), limit_input.size()), limits,
        limit_raw, limit_tokens, frame};
    result = limited.process(limit_input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> two_raw{};
    std::array<LzssTypedToken, 2> two_tokens{};
    LzssContextualRansCompactFrameStreamingEncoder premature{
        stream_config(2, 2), {}, two_raw, two_tokens, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualRansCompactFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssContextualRansCompactFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 32> shared{};
    auto bytes = std::as_writable_bytes(std::span{shared});
    std::array<std::byte, 98> frame{};
    LzssContextualRansCompactFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, bytes.first(1),
        std::span<LzssTypedToken>{shared}.first(1), frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    LzssContextualRansCompactFrameStreamingEncoder output_alias{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualRansCompactFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              ErrorCode::unsupported);

    LzssContextualRansCompactFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
}

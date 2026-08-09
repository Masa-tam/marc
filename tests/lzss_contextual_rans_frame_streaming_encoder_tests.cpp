#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"

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

[[nodiscard]] LzssContextualRansStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    LzssContextualRansStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::array<
    std::byte, lzss_contextual_rans_stream_header_size> stream_header(
    const std::uint8_t frame_size,
    const std::uint8_t original_size) noexcept {
    std::array<std::byte, lzss_contextual_rans_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52}; bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02}; bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01}; bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02}; bytes[16] = std::byte{0x04};
    bytes[18] = std::byte{0x02};
    bytes[20] = static_cast<std::byte>(frame_size);
    bytes[28] = std::byte{0x10}; bytes[32] = std::byte{0x10};
    bytes[40] = static_cast<std::byte>(original_size);
    bytes[48] = std::byte{0x10}; bytes[66] = std::byte{0x01};
    bytes[68] = std::byte{0x05}; bytes[72] = std::byte{0x02};
    bytes[73] = std::byte{0x01}; bytes[80] = std::byte{0x0c};
    bytes[81] = std::byte{0x01}; bytes[82] = std::byte{0x1f};
    bytes[84] = std::byte{0xa6}; bytes[85] = std::byte{0x11};
    bytes[96] = std::byte{0x01}; bytes[98] = std::byte{0x01};
    return bytes;
}

[[nodiscard]] std::vector<std::byte> literal_frame(
    const std::uint8_t sequence) {
    std::vector<std::byte> bytes(9124);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[8] = static_cast<std::byte>(sequence);
    bytes[16] = std::byte{0x01}; bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02}; bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x08}; bytes[36] = std::byte{0x5c};
    bytes[37] = std::byte{0x23}; bytes[64] = std::byte{0x02};
    bytes[68] = std::byte{0x08}; bytes[72] = std::byte{0x0c};
    bytes[74] = std::byte{0x1f}; bytes[76] = std::byte{0xa6};
    bytes[77] = std::byte{0x11}; bytes[80] = std::byte{0x00};
    bytes[81] = std::byte{0x10}; bytes[222] = std::byte{0x00};
    bytes[223] = std::byte{0x10}; bytes[9119] = std::byte{0x80};
    return bytes;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    const auto header = stream_header(1, 2);
    const auto first = literal_frame(0);
    const auto second = literal_frame(1);
    std::vector<std::byte> expected;
    expected.insert(expected.end(), header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualRansFrameStreamingEncoder, MatchesOneByteOracle) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    const auto expected = two_frame_stream();
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 9124> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
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

TEST(LzssContextualRansFrameStreamingEncoder,
     EmitsFullFrameBeforeEndAndFlushKeepsPartialOpen) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 9200> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, frame};
    std::vector<std::byte> output(20000);

    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lzss_contextual_rans_stream_header_size);
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

TEST(LzssContextualRansFrameStreamingEncoder,
     RetainsEndInputAcrossFinalAndEmptyDrain) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 9124> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(1, 1), {}, raw, tokens, frame};
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    const auto header_result = encoder.process({}, header, 0);
    ASSERT_EQ(header_result.status, StreamStatus::progress);
    auto result = encoder.process(input, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 1U);
    std::array<std::byte, 9124> encoded_frame{};
    result = encoder.process({}, encoded_frame, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded_frame.size());

    LzssContextualRansFrameStreamingEncoder empty{
        stream_config(1, 0), {}, {}, {}, {}};
    result = empty.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    result = empty.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
}

TEST(LzssContextualRansFrameStreamingEncoder,
     ReportsCapacityLimitAndInputProtocolFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 9124> frame{};
    std::vector<std::byte> output(10000);

    LzssContextualRansFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens,
        std::span<std::byte>{frame}.first(frame.size() - 1)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_frame.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = 9124;
    LzssContextualRansFrameStreamingEncoder limited{
        stream_config(1, 1), limits, raw, tokens, frame};
    result = limited.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);
    EXPECT_EQ(limited.process({}, {}, 0).error.code,
              ErrorCode::limit_exceeded);

    std::array<std::byte, 2> premature_raw{};
    std::array<LzssTypedToken, 2> premature_tokens{};
    LzssContextualRansFrameStreamingEncoder premature{
        stream_config(2, 2), {}, premature_raw, premature_tokens, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(premature.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualRansFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(too_much.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 800> shared{};
    auto shared_bytes = std::as_writable_bytes(std::span{shared});
    std::array<std::byte, 9124> frame{};
    LzssContextualRansFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, shared_bytes.first(1),
        std::span<LzssTypedToken>{shared}.first(1), frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    LzssContextualRansFrameStreamingEncoder output_alias{
        stream_config(1, 1), {}, raw, tokens, frame};
    EXPECT_EQ(output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualRansFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, frame};
    auto result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              ErrorCode::unsupported);

    LzssContextualRansFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, frame};
    result = reset.process(
        {}, {}, marc::core::flag_value(ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              ErrorCode::unsupported);
}

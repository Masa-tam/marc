#include "frame/lzss_typed_context_frame_streaming_decoder.hpp"

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

[[nodiscard]] constexpr std::array<std::byte, 112> stream_header(
    const std::uint8_t frame_size, const std::uint8_t original_size) {
    std::array<std::byte, 112> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02};
    bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01};
    bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02};
    bytes[16] = std::byte{0x03};
    bytes[18] = std::byte{0x02};
    bytes[20] = static_cast<std::byte>(frame_size);
    bytes[28] = std::byte{0x10};
    bytes[32] = std::byte{0x10};
    bytes[40] = static_cast<std::byte>(original_size);
    bytes[48] = std::byte{0x10};
    bytes[66] = std::byte{0x01};
    bytes[68] = std::byte{0x05};
    bytes[72] = std::byte{0x02};
    bytes[73] = std::byte{0x01};
    bytes[81] = std::byte{0x80};
    bytes[84] = std::byte{0x1f};
    bytes[96] = std::byte{0x01};
    bytes[98] = std::byte{0x01};
    return bytes;
}

[[nodiscard]] constexpr std::array<std::byte, 86> literal_frame(
    const std::uint8_t sequence) {
    std::array<std::byte, 86> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[8] = static_cast<std::byte>(sequence);
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x06};
    bytes[36] = std::byte{0x10};
    bytes[64] = std::byte{0x02};
    bytes[68] = std::byte{0x06};
    bytes[72] = std::byte{0x1f};
    bytes[80] = std::byte{0x00};
    bytes[81] = std::byte{0x20};
    bytes[82] = std::byte{0x7f};
    bytes[83] = std::byte{0xff};
    bytes[84] = std::byte{0xbf};
    bytes[85] = std::byte{0x00};
    return bytes;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    constexpr auto header = stream_header(1, 2);
    constexpr auto first = literal_frame(0);
    constexpr auto second = literal_frame(1);
    std::vector<std::byte> bytes;
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), second.begin(), second.end());
    return bytes;
}

[[nodiscard]] std::vector<std::byte> one_frame_stream() {
    constexpr auto header = stream_header(1, 1);
    constexpr auto frame = literal_frame(0);
    std::vector<std::byte> bytes;
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return bytes;
}

[[nodiscard]] std::vector<std::byte> extended_one_frame_stream() {
    auto header = stream_header(0, 1);
    header[14] = std::byte{0x03};
    header[22] = std::byte{0x10};
    header[66] = std::byte{0x10};
    header[98] = std::byte{0x02};
    constexpr auto frame = literal_frame(0);
    std::vector<std::byte> bytes(header.begin(), header.end());
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return bytes;
}

[[nodiscard]] constexpr std::uint32_t end_flag() {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssTypedContextFrameStreamingDecoder, HandlesOneByteInputAndOutput) {
    const auto encoded = two_frame_stream();
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder decoder{{}, frame, tokens, raw};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, encoded.size() - input_offset);
        const auto chunk = std::span<const std::byte>{encoded}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == encoded.size()
            ? end_flag()
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_EQ(actual, (std::vector{std::byte{'A'}, std::byte{'A'}}));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              StreamStatus::end_of_stream);
}

TEST(LzssTypedContextFrameStreamingDecoder,
     HandlesExtendedVariantWithOneByteInputAndOutput) {
    const auto encoded = extended_one_frame_stream();
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder decoder{{}, frame, tokens, raw};
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, encoded.size() - input_offset);
        const auto chunk = std::span<const std::byte>{encoded}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == encoded.size()
            ? end_flag()
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, StreamStatus::error);
        input_offset += result.input_consumed;
        status = result.status;
    } while (status != StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, encoded.size());
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameStreamingDecoder,
     CommitsOnlyFrameBeforeLaterCorruption) {
    auto encoded = two_frame_stream();
    encoded[112 + 86 + 80] = std::byte{1};
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder decoder{{}, frame, tokens, raw};
    std::array output{std::byte{0xCC}, std::byte{0xCC}};

    const auto result = decoder.process(encoded, output, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xCC});
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              ErrorCode::malformed_stream);
}

TEST(LzssTypedContextFrameStreamingDecoder,
     ReportsWorkspaceAndAggregateFailures) {
    const auto encoded = two_frame_stream();
    std::array<std::byte, 180> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 2> output{};

    LzssTypedContextFrameStreamingDecoder short_frame{
        {}, std::span<std::byte>{frame}.first(85), tokens, raw};
    EXPECT_EQ(short_frame.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssTypedContextFrameStreamingDecoder short_tokens{
        {}, frame, std::span<LzssTypedToken>{tokens}.first(0), raw};
    EXPECT_EQ(short_tokens.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssTypedContextFrameStreamingDecoder short_raw{
        {}, frame, tokens, std::span<std::byte>{raw}.first(0)};
    EXPECT_EQ(short_raw.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    constexpr auto header = stream_header(1, 1);
    constexpr auto canonical = literal_frame(0);
    std::vector<std::byte> expanded(header.begin(), header.end());
    expanded.insert(expanded.end(), canonical.begin(), canonical.end());
    expanded.resize(header.size() + 180, std::byte{0});
    expanded[header.size() + 32] = std::byte{100};
    expanded[header.size() + 68] = std::byte{100};
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes =
        180 + sizeof(LzssTypedToken);
    LzssTypedContextFrameStreamingDecoder aggregate_limited{
        limits, frame, tokens, raw};
    EXPECT_EQ(aggregate_limited.process(
                  expanded, output, end_flag()).error.code,
              ErrorCode::limit_exceeded);

    limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = 1;
    limits.max_frame_size = 1;
    limits.max_block_size = 1;
    LzssTypedContextFrameStreamingDecoder stream_limited{
        limits, frame, tokens, raw};
    EXPECT_EQ(stream_limited.process(
                  encoded, output, end_flag()).error.code,
              ErrorCode::limit_exceeded);

    limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 5;
    LzssTypedContextFrameStreamingDecoder frame_limited{
        limits, frame, tokens, raw};
    EXPECT_EQ(frame_limited.process(
                  encoded, output, end_flag()).error.code,
              ErrorCode::limit_exceeded);
}

TEST(LzssTypedContextFrameStreamingDecoder,
     RejectsTruncationTrailingResetAndOutputAlias) {
    const auto encoded = two_frame_stream();
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 2> output{};

    LzssTypedContextFrameStreamingDecoder truncated{{}, frame, tokens, raw};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    auto trailing_bytes = encoded;
    trailing_bytes.push_back(std::byte{0});
    LzssTypedContextFrameStreamingDecoder trailing{{}, frame, tokens, raw};
    EXPECT_EQ(trailing.process(
                  trailing_bytes, output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    LzssTypedContextFrameStreamingDecoder reset{{}, frame, tokens, raw};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);

    LzssTypedContextFrameStreamingDecoder aliased_output{
        {}, frame, tokens, raw};
    EXPECT_EQ(aliased_output.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssTypedContextFrameStreamingDecoder,
     HandlesEmptyFlushAndPrematureEnd) {
    constexpr auto empty = stream_header(1, 0);
    LzssTypedContextFrameStreamingDecoder empty_decoder{{}, {}, {}, {}};
    EXPECT_EQ(empty_decoder.process(empty, {}, end_flag()).status,
              StreamStatus::end_of_stream);

    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder starved{{}, frame, tokens, raw};
    EXPECT_EQ(starved.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::flush)).status,
              StreamStatus::need_input);

    const auto encoded = two_frame_stream();
    LzssTypedContextFrameStreamingDecoder premature{{}, frame, tokens, raw};
    std::array<std::byte, 1> output{};
    auto result = premature.process(
        std::span<const std::byte>{encoded}.first(112 + 86), output,
        end_flag());
    ASSERT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameStreamingDecoder,
     RejectsOverlappingConstructionWorkspaces) {
    std::array<LzssTypedToken, 8> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    LzssTypedContextFrameStreamingDecoder decoder{
        {}, bytes, std::span<LzssTypedToken>{storage}.first(1), {}};
    const auto result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssTypedContextFrameStreamingDecoder,
     PreservesEndInputAcrossZeroCapacityDrain) {
    const auto encoded = one_frame_stream();
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder decoder{{}, frame, tokens, raw};

    auto result = decoder.process(encoded, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, encoded.size());
    EXPECT_EQ(result.output_produced, 0U);

    std::array<std::byte, 1> output{};
    result = decoder.process({}, output, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameStreamingDecoder, RejectsUnknownFlagsSticky) {
    std::array<std::byte, 86> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssTypedContextFrameStreamingDecoder decoder{{}, frame, tokens, raw};

    auto result = decoder.process({}, {}, UINT32_C(1) << 31);
    ASSERT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
}

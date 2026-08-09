#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"

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

[[nodiscard]] std::array<
    std::byte, lzss_contextual_rans_stream_header_size> stream_header(
    const std::uint8_t frame_size, const std::uint8_t original_size) {
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
    std::vector<std::byte> bytes;
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), second.begin(), second.end());
    return bytes;
}

[[nodiscard]] std::vector<std::byte> one_frame_stream() {
    const auto header = stream_header(1, 1);
    const auto frame = literal_frame(0);
    std::vector<std::byte> bytes;
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return bytes;
}

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] constexpr std::uint32_t end_flag() {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualRansFrameStreamingDecoder, HandlesOneByteInputAndOutput) {
    const auto encoded = two_frame_stream();
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};
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

TEST(LzssContextualRansFrameStreamingDecoder,
     CommitsOnlyFirstFrameBeforeLaterCorruption) {
    auto encoded = two_frame_stream();
    encoded[lzss_contextual_rans_stream_header_size + 9124 + 80]
        = std::byte{1};
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};
    std::array output{std::byte{0xcc}, std::byte{0xcc}};

    const auto result = decoder.process(encoded, output, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xcc});
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              ErrorCode::malformed_stream);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     ReportsWorkspaceAndAggregateFailures) {
    const auto encoded = one_frame_stream();
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};

    LzssContextualRansFrameStreamingDecoder short_frame{
        {}, std::span<std::byte>{frame}.first(frame.size() - 1),
        table_storage, tokens, raw};
    EXPECT_EQ(short_frame.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    EXPECT_EQ(short_frame.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_tables{
        {}, frame,
        std::span<RansDecodeEntry>{table_storage}.first(
            table_storage.size() - 1),
        tokens, raw};
    EXPECT_EQ(short_tables.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    EXPECT_EQ(short_tables.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_tokens{
        {}, frame, table_storage, {}, raw};
    EXPECT_EQ(short_tokens.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_raw{
        {}, frame, table_storage, tokens, {}};
    EXPECT_EQ(short_raw.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    EXPECT_EQ(short_raw.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    const auto aggregate = frame.size()
        + table_storage.size() * sizeof(RansDecodeEntry)
        + sizeof(LzssTypedToken) + raw.size();
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = aggregate - 1;
    LzssContextualRansFrameStreamingDecoder aggregate_limited{
        limits, frame, table_storage, tokens, raw};
    EXPECT_EQ(aggregate_limited.process(
                  encoded, output, end_flag()).error.code,
              ErrorCode::limit_exceeded);
    EXPECT_EQ(aggregate_limited.process({}, {}, 0).error.code,
              ErrorCode::limit_exceeded);

    limits = marc::core::DecoderLimits{};
    limits.max_total_output_size = 1;
    limits.max_frame_size = 1;
    LzssContextualRansFrameStreamingDecoder stream_limited{
        limits, frame, table_storage, tokens, raw};
    EXPECT_EQ(stream_limited.process(
                  two_frame_stream(), output, end_flag()).error.code,
              ErrorCode::limit_exceeded);
    EXPECT_EQ(stream_limited.process({}, {}, 0).error.code,
              ErrorCode::limit_exceeded);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     RejectsTruncationTrailingResetAndOutputAliasSticky) {
    const auto encoded = two_frame_stream();
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 2> output{};

    LzssContextualRansFrameStreamingDecoder truncated{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output, end_flag()).error.code,
              ErrorCode::malformed_stream);
    EXPECT_EQ(truncated.process({}, {}, 0).error.code,
              ErrorCode::malformed_stream);

    auto trailing_bytes = encoded;
    trailing_bytes.push_back(std::byte{0});
    LzssContextualRansFrameStreamingDecoder trailing{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(trailing.process(
                  trailing_bytes, output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    LzssContextualRansFrameStreamingDecoder reset{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code, ErrorCode::unsupported);

    LzssContextualRansFrameStreamingDecoder aliased_output{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(aliased_output.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(aliased_output.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     HandlesEmptyFlushAndPrematureEnd) {
    const auto empty = stream_header(1, 0);
    LzssContextualRansFrameStreamingDecoder empty_decoder{
        {}, {}, {}, {}, {}};
    EXPECT_EQ(empty_decoder.process(empty, {}, end_flag()).status,
              StreamStatus::end_of_stream);

    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder starved{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(starved.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::flush)).status,
              StreamStatus::need_input);

    const auto encoded = two_frame_stream();
    LzssContextualRansFrameStreamingDecoder premature{
        {}, frame, table_storage, tokens, raw};
    std::array<std::byte, 1> output{};
    const auto result = premature.process(
        std::span<const std::byte>{encoded}.first(112 + 9124), output,
        end_flag());
    ASSERT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssContextualRansFrameStreamingDecoder,
     RejectsOverlappingConstructionWorkspaces) {
    auto table_storage = tables();
    auto bytes = std::as_writable_bytes(std::span{table_storage});
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, bytes.first(9124), table_storage, {}, {}};
    const auto result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     PreservesEndInputAcrossZeroCapacityDrain) {
    const auto encoded = one_frame_stream();
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};

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

TEST(LzssContextualRansFrameStreamingDecoder, RejectsUnknownFlagsSticky) {
    std::array<std::byte, 9124> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};

    auto result = decoder.process({}, {}, UINT32_C(1) << 31);
    ASSERT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
}

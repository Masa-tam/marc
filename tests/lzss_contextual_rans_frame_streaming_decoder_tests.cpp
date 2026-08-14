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
    bytes[18] = std::byte{0x03};
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
    std::vector<std::byte> bytes(98);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[8] = static_cast<std::byte>(sequence);
    bytes[16] = std::byte{0x01}; bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02}; bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x08}; bytes[36] = std::byte{0x1a};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    constexpr std::array payload{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    std::ranges::copy(payload, bytes.begin() + 90);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> stream(const std::uint8_t frame_count) {
    const auto header = stream_header(1, frame_count);
    std::vector<std::byte> bytes(header.begin(), header.end());
    for (std::uint8_t sequence = 0; sequence < frame_count; ++sequence) {
        const auto frame = literal_frame(sequence);
        bytes.insert(bytes.end(), frame.begin(), frame.end());
    }
    return bytes;
}

[[nodiscard]] std::vector<RansDecodeEntry> tables() {
    return std::vector<RansDecodeEntry>(contextual_rans_decode_table_entries);
}

[[nodiscard]] constexpr std::uint32_t end_flag() {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualRansFrameStreamingDecoder,
     HandlesOneByteInputAndOutput) {
    const auto encoded = stream(2);
    std::array<std::byte, 98> frame{};
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
    EXPECT_EQ(decoder.process({}, {}, 0).status, StreamStatus::end_of_stream);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     CommitsOnlyFirstFrameBeforeLaterCorruption) {
    auto encoded = stream(2);
    encoded[lzss_contextual_rans_stream_header_size + 98 + 84]
        = std::byte{0x01};
    std::array<std::byte, 98> frame{};
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
    const auto encoded = stream(1);
    std::array<std::byte, 98> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};

    LzssContextualRansFrameStreamingDecoder short_frame{
        {}, std::span<std::byte>{frame}.first(frame.size() - 1),
        table_storage, tokens, raw};
    EXPECT_EQ(short_frame.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_tables{
        {}, frame,
        std::span<RansDecodeEntry>{table_storage}.first(
            table_storage.size() - 1),
        tokens, raw};
    EXPECT_EQ(short_tables.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_tokens{
        {}, frame, table_storage, {}, raw};
    EXPECT_EQ(short_tokens.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingDecoder short_raw{
        {}, frame, table_storage, tokens, {}};
    EXPECT_EQ(short_raw.process(encoded, output, end_flag()).error.code,
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
    EXPECT_EQ(aggregate_limited.process(encoded, output, end_flag()).error.code,
              ErrorCode::limit_exceeded);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     RejectsTruncationTrailingFlagsAndAliasesSticky) {
    const auto encoded = stream(1);
    std::array<std::byte, 98> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};

    LzssContextualRansFrameStreamingDecoder truncated{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    auto trailing = encoded;
    trailing.push_back(std::byte{0});
    LzssContextualRansFrameStreamingDecoder trailing_decoder{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(trailing_decoder.process(
                  trailing, output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    LzssContextualRansFrameStreamingDecoder flags{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(flags.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
    EXPECT_EQ(flags.process({}, {}, 0).error.code, ErrorCode::unsupported);

    LzssContextualRansFrameStreamingDecoder unknown{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code, ErrorCode::unsupported);

    LzssContextualRansFrameStreamingDecoder aliased_output{
        {}, frame, table_storage, tokens, raw};
    EXPECT_EQ(aliased_output.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);

    auto table_bytes = std::as_writable_bytes(std::span{table_storage});
    LzssContextualRansFrameStreamingDecoder aliased_construction{
        {}, table_bytes.first(frame.size()), table_storage, {}, {}};
    EXPECT_EQ(aliased_construction.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     HandlesEmptyAndPreservesEndAcrossDrain) {
    const auto empty = stream_header(1, 0);
    LzssContextualRansFrameStreamingDecoder empty_decoder{
        {}, {}, {}, {}, {}};
    EXPECT_EQ(empty_decoder.process(empty, {}, end_flag()).status,
              StreamStatus::end_of_stream);

    const auto encoded = stream(1);
    std::array<std::byte, 98> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};
    auto result = decoder.process(encoded, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, encoded.size());

    std::array<std::byte, 1> output{};
    result = decoder.process({}, output, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssContextualRansFrameStreamingDecoder,
     RejectsRetiredVariantTwoStreamIdentity) {
    auto encoded = stream(1);
    encoded[18] = std::byte{0x02};
    std::array<std::byte, 98> frame{};
    auto table_storage = tables();
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, frame, table_storage, tokens, raw};

    const auto result = decoder.process(encoded, output, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
}

TEST(LzssContextualRansFrameStreamingDecoder,
     KeepsOneMiBIdentityUnavailableUntilLifecycleAdmission) {
    auto encoded = stream_header(1, 0);
    encoded[14] = std::byte{0x03};
    encoded[66] = std::byte{0x10};
    encoded[84] = std::byte{0xc6};
    encoded[85] = std::byte{0x11};
    encoded[98] = std::byte{0x02};
    LzssContextualRansFrameStreamingDecoder decoder{{}, {}, {}, {}, {}};
    const auto result = decoder.process(encoded, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
}

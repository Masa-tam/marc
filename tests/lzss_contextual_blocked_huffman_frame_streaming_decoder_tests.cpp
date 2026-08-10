#include "frame/lzss_contextual_blocked_huffman_frame_streaming_decoder.hpp"

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
using marc::entropy::internal::ContextualBlockedHuffmanDescriptor;
using marc::entropy::internal::HuffmanDecodeTable;
using marc::entropy::internal::
    contextual_blocked_huffman_no_single_symbol;

[[nodiscard]] constexpr std::array<std::byte, 24> descriptor_bytes(
    const std::uint32_t decision_count) {
    return {
        static_cast<std::byte>(decision_count & 0xffU),
        static_cast<std::byte>((decision_count >> 8U) & 0xffU),
        static_cast<std::byte>((decision_count >> 16U) & 0xffU),
        static_cast<std::byte>((decision_count >> 24U) & 0xffU),
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{15}, std::byte{3}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{65}, std::byte{0}};
}

[[nodiscard]] std::vector<std::byte> literal_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const std::uint64_t sequence, const std::uint32_t literal_count) {
    std::vector<std::byte> frame(88);
    const LzssContextualBlockedHuffmanFrameHeader header{
        0, sequence, literal_count, literal_count, 2U * literal_count,
        2U * literal_count, 0, 24, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_frame_header(
                  header, {stream, {}, sequence, sequence},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    std::ranges::copy(
        descriptor_bytes(2U * literal_count), frame.begin() + 64);
    return frame;
}

[[nodiscard]] std::vector<std::byte> stream(const std::uint32_t frame_count) {
    LzssContextualBlockedHuffmanStreamHeader config{};
    config.frame_size = 1;
    config.original_size = frame_count;
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        header{};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  config, {}, header),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    std::vector<std::byte> bytes(header.begin(), header.end());
    for (std::uint32_t sequence = 0; sequence < frame_count; ++sequence) {
        const auto frame = literal_frame(config, sequence, 1);
        bytes.insert(bytes.end(), frame.begin(), frame.end());
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte> one_large_frame(
    const std::uint32_t literal_count) {
    LzssContextualBlockedHuffmanStreamHeader config{};
    config.frame_size = literal_count;
    config.original_size = literal_count;
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        header{};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  config, {}, header),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    std::vector<std::byte> bytes(header.begin(), header.end());
    const auto frame = literal_frame(config, 0, literal_count);
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return bytes;
}

[[nodiscard]] std::vector<std::byte> literal_match_stream() {
    LzssContextualBlockedHuffmanStreamHeader config{};
    config.frame_size = 6;
    config.original_size = 6;
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        stream_bytes{};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  config, {}, stream_bytes),
              LzssContextualBlockedHuffmanStreamHeaderError::none);

    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 5;
    descriptor.payload_size = 1;
    descriptor.final_valid_bits = 2;
    descriptor.field_active_mask = 0x0F;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    descriptor.field_models[0].lengths[0] = 1;
    descriptor.field_models[0].lengths[1] = 1;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    descriptor.field_models[2].active = true;
    descriptor.field_models[2].single_symbol = 0;
    descriptor.field_models[3].active = true;
    descriptor.field_models[3].single_symbol = 0;
    std::array<std::byte, 2561> descriptor_bytes{};
    std::size_t descriptor_size{};
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 5, 1, {}, descriptor_bytes, descriptor_size),
              marc::entropy::internal::ContextualBlockedHuffmanFormatError::
                  none);

    std::vector<std::byte> frame(
        lzss_contextual_blocked_huffman_frame_header_size + descriptor_size
        + 1);
    const LzssContextualBlockedHuffmanFrameHeader header{
        0, 0, 6, 2, 5, 5, 1, static_cast<std::uint32_t>(descriptor_size),
        0, 0};
    EXPECT_EQ(serialize_lzss_contextual_blocked_huffman_frame_header(
                  header, {config, {}, 0, 0},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualBlockedHuffmanFrameHeaderError::none);
    std::ranges::copy_n(
        descriptor_bytes.begin(), descriptor_size, frame.begin() + 64);
    frame.back() = std::byte{0x02};
    std::vector<std::byte> result(stream_bytes.begin(), stream_bytes.end());
    result.insert(result.end(), frame.begin(), frame.end());
    return result;
}

[[nodiscard]] constexpr std::uint32_t end_flag() {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     HandlesOneByteInputAndOutputWithoutTables) {
    const auto encoded = stream(2);
    std::array<std::byte, 88> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame, {}, tokens, raw};
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
            : marc::core::flag_value(ProcessFlags::flush);
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

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     CommitsOnlyFirstFrameBeforeLaterCorruption) {
    auto encoded = stream(2);
    encoded[lzss_contextual_blocked_huffman_stream_header_size + 88 + 84]
        = std::byte{1};
    std::array<std::byte, 88> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame, {}, tokens, raw};
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

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     ReportsWorkspaceAndAggregateFailures) {
    const auto encoded = stream(1);
    std::array<std::byte, 88> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};

    LzssContextualBlockedHuffmanFrameStreamingDecoder short_frame{
        {}, std::span<std::byte>{frame}.first(87), {}, tokens, raw};
    EXPECT_EQ(short_frame.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    LzssContextualBlockedHuffmanFrameStreamingDecoder short_tokens{
        {}, frame, {}, {}, raw};
    EXPECT_EQ(short_tokens.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);
    LzssContextualBlockedHuffmanFrameStreamingDecoder short_raw{
        {}, frame, {}, tokens, {}};
    EXPECT_EQ(short_raw.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    constexpr std::uint32_t count = 8;
    const auto large = one_large_frame(count);
    std::array<LzssTypedToken, count> large_tokens{};
    std::array<std::byte, count> large_raw{};
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = count;
    limits.max_block_size = count;
    const auto aggregate = frame.size()
        + large_tokens.size() * sizeof(LzssTypedToken) + large_raw.size();
    limits.max_internal_buffered_bytes = aggregate - 1;
    LzssContextualBlockedHuffmanFrameStreamingDecoder aggregate_limited{
        limits, frame, {}, large_tokens, large_raw};
    EXPECT_EQ(
        aggregate_limited.process(large, output, end_flag()).error.code,
        ErrorCode::limit_exceeded);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     DerivesExactNonSingleTableWorkspace) {
    const auto encoded = literal_match_stream();
    std::array<std::byte, 2561 + 65> frame{};
    std::array<HuffmanDecodeTable, 1> tables{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 6> raw{};
    std::array<std::byte, 6> output{};

    LzssContextualBlockedHuffmanFrameStreamingDecoder short_tables{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(short_tables.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame, tables, tokens, raw};
    const auto decoded = decoder.process(encoded, output, end_flag());
    ASSERT_EQ(decoded.status, StreamStatus::end_of_stream);
    ASSERT_EQ(decoded.output_produced, output.size());
    EXPECT_TRUE(std::ranges::all_of(output, [](const auto value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     RejectsTruncationTrailingFlagsAndAliasesSticky) {
    const auto encoded = stream(1);
    std::array<std::byte, 88> frame{};
    std::array<HuffmanDecodeTable, 1> tables{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};

    LzssContextualBlockedHuffmanFrameStreamingDecoder truncated{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(truncated.process(
                  std::span<const std::byte>{encoded}.first(encoded.size() - 1),
                  output, end_flag()).error.code,
              ErrorCode::malformed_stream);
    auto trailing = encoded;
    trailing.push_back(std::byte{0});
    LzssContextualBlockedHuffmanFrameStreamingDecoder trailing_decoder{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(trailing_decoder.process(trailing, output, end_flag()).error.code,
              ErrorCode::malformed_stream);

    LzssContextualBlockedHuffmanFrameStreamingDecoder flags{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(flags.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
    EXPECT_EQ(flags.process({}, {}, 0).error.code, ErrorCode::unsupported);
    LzssContextualBlockedHuffmanFrameStreamingDecoder unknown{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              ErrorCode::unsupported);

    LzssContextualBlockedHuffmanFrameStreamingDecoder raw_alias{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(raw_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    LzssContextualBlockedHuffmanFrameStreamingDecoder frame_alias{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(frame_alias.process(
                  {}, std::span<std::byte>{frame}.first(1), 0).error.code,
              ErrorCode::invalid_argument);
    LzssContextualBlockedHuffmanFrameStreamingDecoder table_alias{
        {}, frame, tables, tokens, raw};
    EXPECT_EQ(table_alias.process(
                  {}, std::as_writable_bytes(std::span{tables}).first(1), 0)
                  .error.code,
              ErrorCode::invalid_argument);
    LzssContextualBlockedHuffmanFrameStreamingDecoder token_alias{
        {}, frame, {}, tokens, raw};
    EXPECT_EQ(token_alias.process(
                  {}, std::as_writable_bytes(std::span{tokens}).first(1), 0)
                  .error.code,
              ErrorCode::invalid_argument);

    auto table_bytes = std::as_writable_bytes(std::span{tables});
    LzssContextualBlockedHuffmanFrameStreamingDecoder construction_alias{
        {}, table_bytes.first(frame.size()), tables, {}, {}};
    EXPECT_EQ(construction_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     HandlesEmptyAndPreservesEndAcrossDrain) {
    LzssContextualBlockedHuffmanStreamHeader empty_config{};
    empty_config.frame_size = 1;
    std::array<std::byte,
               lzss_contextual_blocked_huffman_stream_header_size>
        empty{};
    ASSERT_EQ(serialize_lzss_contextual_blocked_huffman_stream_header(
                  empty_config, {}, empty),
              LzssContextualBlockedHuffmanStreamHeaderError::none);
    LzssContextualBlockedHuffmanFrameStreamingDecoder empty_decoder{
        {}, {}, {}, {}, {}};
    EXPECT_EQ(empty_decoder.process(empty, {}, end_flag()).status,
              StreamStatus::end_of_stream);

    const auto encoded = stream(1);
    std::array<std::byte, 88> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame, {}, tokens, raw};
    auto result = decoder.process(encoded, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, encoded.size());
    std::array<std::byte, 1> output{};
    result = decoder.process({}, output, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});
}

TEST(LzssContextualBlockedHuffmanFrameStreamingDecoder,
     RejectsWrongEntropyIdentity) {
    auto encoded = stream(1);
    encoded[16] = std::byte{4};
    std::array<std::byte, 88> frame{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
    std::array<std::byte, 1> output{};
    LzssContextualBlockedHuffmanFrameStreamingDecoder decoder{
        {}, frame, {}, tokens, raw};
    const auto result = decoder.process(encoded, output, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
}

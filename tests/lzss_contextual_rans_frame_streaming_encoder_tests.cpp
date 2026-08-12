#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"

#include "frame/lzss_contextual_rans_frame_encoder.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"
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
using marc::entropy::internal::RansDecodeEntry;
using marc::entropy::internal::contextual_rans_decode_table_entries;

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

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
    LzssContextualRansFrameStreamingEncoder& encoder,
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
    EXPECT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, {}, header),
              LzssContextualRansStreamHeaderError::none);
    EXPECT_EQ(header[18], std::byte{0x03});
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> first{};
    std::array<std::byte, 98> second{};
    EXPECT_EQ(encode_lzss_contextual_rans_frame(
                  stream, {}, 0, 0, raw, tokens, first).error,
              LzssContextualRansFrameEncodeError::none);
    EXPECT_EQ(encode_lzss_contextual_rans_frame(
                  stream, {}, 1, 1, raw, tokens, second).error,
              LzssContextualRansFrameEncodeError::none);
    std::vector<std::byte> expected;
    expected.insert(expected.end(), header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

} // namespace

TEST(LzssContextualRansFrameStreamingEncoder,
     MatchesCanonicalOracleWithOneByteChunks) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, {}, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);
    EXPECT_EQ(encoded, two_frame_oracle());
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              StreamStatus::end_of_stream);
}

TEST(LzssContextualRansFrameStreamingEncoder,
     CanonicalStreamingDecoderRecoversOutput) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, {}, frame};
    const auto encoded = encode_one_byte_chunks(encoder, input);

    std::array<std::byte, 98> serialized{};
    std::vector<RansDecodeEntry> tables(
        contextual_rans_decode_table_entries);
    std::array<LzssTypedToken, 1> decode_tokens{};
    std::array<std::byte, 1> decode_raw{};
    LzssContextualRansFrameStreamingDecoder decoder{
        {}, serialized, tables, decode_tokens, decode_raw};
    std::array<std::byte, input.size()> decoded{};
    const auto result = decoder.process(encoded, decoded, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, encoded.size());
    EXPECT_EQ(result.output_produced, decoded.size());
    EXPECT_EQ(decoded, input);
}

TEST(LzssContextualRansFrameStreamingEncoder,
     HashChainMatchesExhaustiveStreamAndValidatesWorkspace) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'3'}};
    const auto stream = stream_config(input.size(), input.size());
    const auto finder_requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(input.size(), stream.dictionary,
                                            {});
    ASSERT_EQ(finder_requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    ASSERT_GT(finder_requirements.workspace_size, 0U);
    AlignedWorkspace finder_owner{finder_requirements.workspace_size};
    auto finder = finder_owner.bytes(finder_requirements.workspace_size);

    std::array<LzssTypedToken, input.size()> oracle_tokens{};
    std::vector<std::byte> oracle_frame(16'384);
    const auto oracle = encode_lzss_contextual_rans_frame(
        stream, {}, 0, 0, input, oracle_tokens, oracle_frame);
    ASSERT_EQ(oracle.error, LzssContextualRansFrameEncodeError::none);
    oracle_frame.resize(oracle.serialized_size);
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    ASSERT_EQ(serialize_lzss_contextual_rans_stream_header(
                  stream, {}, header),
              LzssContextualRansStreamHeaderError::none);
    std::vector<std::byte> expected(header.begin(), header.end());
    expected.insert(expected.end(), oracle_frame.begin(), oracle_frame.end());

    std::array<std::byte, input.size()> raw{};
    std::array<LzssTypedToken, input.size()> tokens{};
    std::vector<std::byte> frame(16'384);
    LzssContextualRansFrameStreamingEncoder encoder{
        stream, {}, raw, tokens, finder, frame};
    std::vector<std::byte> actual(expected.size());
    const auto result = encoder.process(input, actual, end_flag());
    ASSERT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, input.size());
    EXPECT_EQ(result.output_produced, expected.size());
    EXPECT_EQ(actual, expected);

    LzssContextualRansFrameStreamingEncoder short_finder{
        stream, {}, raw, tokens, finder.first(finder.size() - 1), frame};
    std::vector<std::byte> failure_output(expected.size());
    const auto short_result = short_finder.process(
        input, failure_output, end_flag());
    EXPECT_EQ(short_result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_result.input_consumed, input.size());
    EXPECT_EQ(short_result.output_produced,
              lzss_contextual_rans_stream_header_size);

    LzssContextualRansFrameStreamingEncoder alias{
        stream, {}, raw, tokens, raw, frame};
    EXPECT_EQ(alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingEncoder,
     FlushKeepsPartialFrameOpenAndEndInputSurvivesDrain) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<std::byte, 256> frame{};
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, {}, frame};
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

TEST(LzssContextualRansFrameStreamingEncoder,
     EmptyInputEndsAfterHeaderDrain) {
    LzssContextualRansFrameStreamingEncoder encoder{
        stream_config(1, 0), {}, {}, {}, {}, {}};
    auto result = encoder.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    std::array<std::byte, lzss_contextual_rans_stream_header_size> header{};
    result = encoder.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
    EXPECT_EQ(header[18], std::byte{0x03});
}

TEST(LzssContextualRansFrameStreamingEncoder,
     ReportsCapacityLimitAndInputFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 98> frame{};
    std::vector<std::byte> output(256);

    LzssContextualRansFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, {}, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualRansFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens, {},
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
    const auto limit_finder_requirements = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            limit_input.size(), stream_config(8, 8).dictionary, {});
    ASSERT_EQ(limit_finder_requirements.error,
              marc::dictionary::internal::LzssHashChainError::none);
    AlignedWorkspace limit_finder_owner{
        limit_finder_requirements.workspace_size};
    LzssContextualRansFrameStreamingEncoder limited{
        stream_config(limit_input.size(), limit_input.size()), limits,
        limit_raw, limit_tokens,
        limit_finder_owner.bytes(limit_finder_requirements.workspace_size),
        frame};
    result = limited.process(limit_input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> two_raw{};
    std::array<LzssTypedToken, 2> two_tokens{};
    LzssContextualRansFrameStreamingEncoder premature{
        stream_config(2, 2), {}, two_raw, two_tokens, {}, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualRansFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, {}, frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssContextualRansFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 32> shared{};
    auto bytes = std::as_writable_bytes(std::span{shared});
    std::array<std::byte, 98> frame{};
    LzssContextualRansFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, bytes.first(1),
        std::span<LzssTypedToken>{shared}.first(1), {}, frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    LzssContextualRansFrameStreamingEncoder output_alias{
        stream_config(1, 1), {}, raw, tokens, {}, frame};
    EXPECT_EQ(output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualRansFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, {}, frame};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              ErrorCode::unsupported);

    LzssContextualRansFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, {}, frame};
    EXPECT_EQ(reset.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
}

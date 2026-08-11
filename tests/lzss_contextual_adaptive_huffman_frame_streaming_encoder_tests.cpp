#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::core::ErrorCode;
using marc::core::ProcessFlags;
using marc::core::StreamStatus;
using marc::dictionary::internal::LzssTypedToken;
using marc::entropy::internal::AdaptiveHuffmanNode;
using marc::entropy::internal::contextual_adaptive_huffman_node_entries;
using marc::entropy::internal::contextual_adaptive_huffman_symbol_entries;

struct ModelWorkspace {
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::array<
    std::byte, lzss_contextual_adaptive_huffman_stream_header_size>
stream_header(const std::uint32_t frame_size,
              const std::uint64_t original_size) noexcept {
    std::array<
        std::byte, lzss_contextual_adaptive_huffman_stream_header_size> bytes{};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream_config(frame_size, original_size), {}, bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> literal_frame(
    const std::uint64_t sequence, const std::uint64_t original_size) {
    std::vector<std::byte> bytes(82);
    const auto stream = stream_config(1, original_size);
    const LzssContextualAdaptiveHuffmanFrameHeader header{
        0, sequence, 1, 1, 2, 2, 2, 16, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, sequence, sequence},
                  std::span<std::byte, 64>{bytes.data(), 64}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    bytes[80] = std::byte{0x82};
    bytes[81] = std::byte{0x00};
    return bytes;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    const auto header = stream_header(1, 2);
    const auto first = literal_frame(0, 2);
    const auto second = literal_frame(1, 2);
    std::vector<std::byte> expected(header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanFrameStreamingEncoder,
     MatchesOneByteOracle) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    const auto expected = two_frame_stream();
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    ModelWorkspace models{};
    std::array<std::byte, 82> frame{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, models.nodes,
        models.symbols, frame};

    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    StreamStatus status{};
    do {
        const auto count =
            std::min<std::size_t>(1, input.size() - input_offset);
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

TEST(LzssContextualAdaptiveHuffmanFrameStreamingEncoder,
     EmitsFullFrameBeforeEndAndFlushKeepsPartialOpen) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    ModelWorkspace models{};
    std::array<std::byte, 256> frame{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, models.nodes,
        models.symbols, frame};
    std::vector<std::byte> output(1024);

    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lzss_contextual_adaptive_huffman_stream_header_size);
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

TEST(LzssContextualAdaptiveHuffmanFrameStreamingEncoder,
     RetainsEndInputAcrossFinalAndEmptyDrain) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    ModelWorkspace models{};
    std::array<std::byte, 82> frame{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder encoder{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size> header{};
    const auto header_result = encoder.process({}, header, 0);
    ASSERT_EQ(header_result.status, StreamStatus::progress);
    auto result = encoder.process(input, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 1U);
    std::array<std::byte, 82> encoded_frame{};
    result = encoder.process({}, encoded_frame, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded_frame.size());

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder empty{
        stream_config(1, 0), {}, {}, {}, {}, {}, {}};
    result = empty.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    result = empty.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingEncoder,
     ReportsCapacityLimitAndInputProtocolFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    ModelWorkspace models{};
    std::array<std::byte, 82> frame{};
    std::vector<std::byte> output(512);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, models.nodes, models.symbols, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder short_nodes{
        stream_config(1, 1), {}, raw, tokens,
        std::span{models.nodes}.first(models.nodes.size() - 1), models.symbols,
        frame};
    result = short_nodes.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder short_symbols{
        stream_config(1, 1), {}, raw, tokens, models.nodes,
        std::span{models.symbols}.first(models.symbols.size() - 1), frame};
    result = short_symbols.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        std::span<std::byte>{frame}.first(frame.size() - 1)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        raw.size() + sizeof(LzssTypedToken)
        + models.nodes.size() * sizeof(AdaptiveHuffmanNode)
        + models.symbols.size() * sizeof(std::uint16_t) + frame.size() - 1;
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder limited{
        stream_config(1, 1), limits, raw, tokens, models.nodes,
        models.symbols, frame};
    result = limited.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> premature_raw{};
    std::array<LzssTypedToken, 2> premature_tokens{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder premature{
        stream_config(2, 2), {}, premature_raw, premature_tokens, models.nodes,
        models.symbols, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    ModelWorkspace models{};
    std::array<LzssTypedToken, 1> shared{};
    auto shared_bytes = std::as_writable_bytes(std::span{shared});
    std::array<std::byte, 82> frame{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, shared_bytes.first(1), shared, models.nodes,
        models.symbols, frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto node_bytes = std::as_writable_bytes(std::span{models.nodes});
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder raw_node_overlap{
        stream_config(1, 1), {}, node_bytes.first(1), tokens, models.nodes,
        models.symbols, frame};
    EXPECT_EQ(raw_node_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    auto symbol_bytes = std::as_writable_bytes(std::span{models.symbols});
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder raw_symbol_overlap{
        stream_config(1, 1), {}, symbol_bytes.first(1), tokens, models.nodes,
        models.symbols, frame};
    EXPECT_EQ(raw_symbol_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 82> raw_frame_shared{};
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder raw_frame_overlap{
        stream_config(1, 1), {},
        std::span<std::byte>{raw_frame_shared}.first(1), tokens, models.nodes,
        models.symbols, raw_frame_shared};
    EXPECT_EQ(raw_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    LzssContextualAdaptiveHuffmanFrameStreamingEncoder token_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        token_bytes};
    EXPECT_EQ(token_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder node_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        node_bytes.first(82)};
    EXPECT_EQ(node_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder symbol_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        symbol_bytes.first(82)};
    EXPECT_EQ(symbol_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder raw_output_alias{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    EXPECT_EQ(raw_output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(raw_output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder token_output_alias{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    EXPECT_EQ(token_output_alias.process({}, token_bytes, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder node_output_alias{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    EXPECT_EQ(node_output_alias.process({}, node_bytes.first(1), 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder symbol_output_alias{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    EXPECT_EQ(symbol_output_alias.process(
                  {}, symbol_bytes.first(1), 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder frame_output_alias{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    EXPECT_EQ(frame_output_alias.process({}, frame, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    auto result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              ErrorCode::unsupported);

    LzssContextualAdaptiveHuffmanFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, models.nodes, models.symbols,
        frame};
    result = reset.process(
        {}, {}, marc::core::flag_value(ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              ErrorCode::unsupported);
}

#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_decoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

struct Workspace {
    std::vector<std::byte> frame = std::vector<std::byte>(82);
    std::vector<AdaptiveHuffmanNode> nodes =
        std::vector<AdaptiveHuffmanNode>(
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> symbols = std::vector<std::uint16_t>(
        contextual_adaptive_huffman_symbol_entries);
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};
};

[[nodiscard]] LzssContextualAdaptiveHuffmanStreamHeader stream_config(
    const std::uint64_t original_size = 1,
    const std::uint32_t frame_size = 64) {
    LzssContextualAdaptiveHuffmanStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> frame_vector(
    const LzssContextualAdaptiveHuffmanStreamHeader& stream = stream_config(),
    const std::uint64_t sequence = 0,
    const std::uint64_t output_committed = 0) {
    std::vector<std::byte> frame(82);
    const LzssContextualAdaptiveHuffmanFrameHeader header{
        0, sequence, 1, 1, 2, 2, 2, 16, 0, 0};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_frame_header(
                  header, {stream, {}, sequence, output_committed},
                  std::span<std::byte, 64>{frame.data(), 64}),
              LzssContextualAdaptiveHuffmanFrameHeaderError::none);
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    std::ranges::copy(descriptor, frame.begin() + 64);
    frame[80] = std::byte{0x82};
    frame[81] = std::byte{0x00};
    return frame;
}

[[nodiscard]] std::vector<std::byte> stream(
    const std::uint64_t original_size = 1) {
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        header{};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  stream_config(original_size), {}, header),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    std::vector<std::byte> result(header.begin(), header.end());
    if (original_size != 0) {
        const auto frame = frame_vector();
        result.insert(result.end(), frame.begin(), frame.end());
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    const auto config = stream_config(2, 1);
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        header{};
    EXPECT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  config, {}, header),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    std::vector<std::byte> result(header.begin(), header.end());
    for (std::uint64_t sequence = 0; sequence < 2; ++sequence) {
        const auto frame = frame_vector(config, sequence, sequence);
        result.insert(result.end(), frame.begin(), frame.end());
    }
    return result;
}

[[nodiscard]] constexpr std::uint32_t end_flag() {
    return marc::core::flag_value(ProcessFlags::end_input);
}

} // namespace

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     HandlesOneByteInputAndOutputDeterministically) {
    const auto encoded = stream();
    Workspace workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, workspace.frame, workspace.nodes, workspace.symbols,
        workspace.tokens, workspace.raw};
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    std::vector<std::byte> actual;
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
    EXPECT_EQ(actual, (std::vector{std::byte{'A'}}));
    EXPECT_EQ(decoder.process({}, {}, 0).status, StreamStatus::end_of_stream);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     PreservesEndInputWhileDrainingAndHandlesEmptyStream) {
    Workspace workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, workspace.frame, workspace.nodes, workspace.symbols,
        workspace.tokens, workspace.raw};
    const auto encoded = stream();
    auto result = decoder.process(encoded, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, encoded.size());
    std::array<std::byte, 1> output{};
    result = decoder.process({}, output, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 1U);
    EXPECT_EQ(output[0], std::byte{'A'});

    Workspace empty_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder empty_decoder{
        {}, empty_workspace.frame, empty_workspace.nodes,
        empty_workspace.symbols, empty_workspace.tokens, empty_workspace.raw};
    const auto empty = stream(0);
    EXPECT_EQ(empty_decoder.process(empty, {}, end_flag()).status,
              StreamStatus::end_of_stream);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     ResetsModelsAndAcceptsTheNextFrame) {
    const auto encoded = two_frame_stream();
    Workspace workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, workspace.frame, workspace.nodes, workspace.symbols,
        workspace.tokens, workspace.raw};
    std::array<std::byte, 2> output{};
    const auto result = decoder.process(encoded, output, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, encoded.size());
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(output, (std::array{std::byte{'A'}, std::byte{'A'}}));
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     RejectsTruncationTrailingDataAndMalformedPayloadWithoutPublishing) {
    const auto encoded = stream();
    std::array<std::byte, 1> output{std::byte{0xCC}};

    constexpr std::array truncation_points{
        lzss_contextual_adaptive_huffman_stream_header_size - 1,
        lzss_contextual_adaptive_huffman_stream_header_size
            + lzss_contextual_adaptive_huffman_frame_header_size - 1,
        std::size_t{193}};
    for (const auto extent : truncation_points) {
        Workspace truncated_workspace{};
        LzssContextualAdaptiveHuffmanFrameStreamingDecoder truncated{
            {}, truncated_workspace.frame, truncated_workspace.nodes,
            truncated_workspace.symbols, truncated_workspace.tokens,
            truncated_workspace.raw};
        EXPECT_EQ(truncated.process(
                      std::span<const std::byte>{encoded}.first(extent),
                      output, end_flag()).error.code,
                  ErrorCode::malformed_stream);
        EXPECT_EQ(output[0], std::byte{0xCC});
    }

    auto trailing_bytes = encoded;
    trailing_bytes.push_back(std::byte{0});
    Workspace trailing_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder trailing{
        {}, trailing_workspace.frame, trailing_workspace.nodes,
        trailing_workspace.symbols, trailing_workspace.tokens,
        trailing_workspace.raw};
    EXPECT_EQ(trailing.process(trailing_bytes, output, end_flag()).error.code,
              ErrorCode::malformed_stream);
    EXPECT_EQ(output[0], std::byte{0xCC});

    auto malformed = encoded;
    malformed.back() = std::byte{0x80};
    Workspace malformed_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder malformed_decoder{
        {}, malformed_workspace.frame, malformed_workspace.nodes,
        malformed_workspace.symbols, malformed_workspace.tokens,
        malformed_workspace.raw};
    EXPECT_EQ(malformed_decoder.process(malformed, output, end_flag())
                  .error.code,
              ErrorCode::malformed_stream);
    EXPECT_EQ(output[0], std::byte{0xCC});
    EXPECT_EQ(malformed_decoder.process({}, {}, 0).error.code,
              ErrorCode::malformed_stream);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     ReportsWorkspaceAndAggregateFailures) {
    const auto encoded = stream();
    std::array<std::byte, 1> output{};

    Workspace short_model{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder short_nodes{
        {}, short_model.frame,
        std::span{short_model.nodes}.first(short_model.nodes.size() - 1),
        short_model.symbols, short_model.tokens, short_model.raw};
    EXPECT_EQ(short_nodes.process(encoded, output, end_flag()).error.code,
              ErrorCode::invalid_argument);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder short_symbols{
        {}, short_model.frame, short_model.nodes,
        std::span{short_model.symbols}.first(short_model.symbols.size() - 1),
        short_model.tokens, short_model.raw};
    EXPECT_EQ(short_symbols.process(encoded, output, end_flag()).error.code,
              ErrorCode::invalid_argument);

    Workspace short_frame_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder short_frame{
        {}, std::span{short_frame_workspace.frame}.first(81),
        short_frame_workspace.nodes, short_frame_workspace.symbols,
        short_frame_workspace.tokens, short_frame_workspace.raw};
    EXPECT_EQ(short_frame.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    Workspace short_token_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder short_tokens{
        {}, short_token_workspace.frame, short_token_workspace.nodes,
        short_token_workspace.symbols, {}, short_token_workspace.raw};
    EXPECT_EQ(short_tokens.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    Workspace short_raw_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder short_raw{
        {}, short_raw_workspace.frame, short_raw_workspace.nodes,
        short_raw_workspace.symbols, short_raw_workspace.tokens, {}};
    EXPECT_EQ(short_raw.process(encoded, output, end_flag()).error.code,
              ErrorCode::out_of_memory);

    Workspace aggregate_workspace{};
    auto limits = marc::core::DecoderLimits{};
    const auto aggregate = aggregate_workspace.frame.size()
        + aggregate_workspace.nodes.size() * sizeof(AdaptiveHuffmanNode)
        + aggregate_workspace.symbols.size() * sizeof(std::uint16_t)
        + sizeof(LzssTypedToken) + aggregate_workspace.raw.size();
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes = aggregate - 1;
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder aggregate_limited{
        limits, aggregate_workspace.frame, aggregate_workspace.nodes,
        aggregate_workspace.symbols, aggregate_workspace.tokens,
        aggregate_workspace.raw};
    EXPECT_EQ(aggregate_limited.process(encoded, output, end_flag())
                  .error.code,
              ErrorCode::limit_exceeded);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     RejectsFlagsAndOutputAliasesWithStickyErrors) {
    Workspace flags_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder flags{
        {}, flags_workspace.frame, flags_workspace.nodes,
        flags_workspace.symbols, flags_workspace.tokens, flags_workspace.raw};
    EXPECT_EQ(flags.process(
                  {}, {}, marc::core::flag_value(ProcessFlags::reset_block))
                  .error.code,
              ErrorCode::unsupported);
    EXPECT_EQ(flags.process({}, {}, 0).error.code, ErrorCode::unsupported);

    Workspace unknown_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder unknown{
        {}, unknown_workspace.frame, unknown_workspace.nodes,
        unknown_workspace.symbols, unknown_workspace.tokens,
        unknown_workspace.raw};
    EXPECT_EQ(unknown.process({}, {}, UINT32_C(1) << 31).error.code,
              ErrorCode::unsupported);

    Workspace alias_workspace{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder alias{
        {}, alias_workspace.frame, alias_workspace.nodes,
        alias_workspace.symbols, alias_workspace.tokens, alias_workspace.raw};
    EXPECT_EQ(alias.process({}, alias_workspace.raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     RejectsOverlappingConstructorWorkspaces) {
    Workspace workspace{};
    const auto node_bytes = std::as_writable_bytes(
        std::span{workspace.nodes});
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, node_bytes.first(82), workspace.nodes, workspace.symbols,
        workspace.tokens, workspace.raw};
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     EnforcesExplicitProfileAdmissionBeforeFrames) {
    LzssContextualAdaptiveHuffmanStreamHeader baseline{};
    baseline.frame_size = 1;
    baseline.original_size = 0;
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        baseline_bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  baseline, {}, baseline_bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);

    std::array<marc::entropy::internal::AdaptiveHuffmanNode,
               marc::entropy::internal::
                   contextual_adaptive_huffman_node_entries_v2>
        nodes{};
    std::array<std::uint16_t,
               marc::entropy::internal::
                   contextual_adaptive_huffman_symbol_entries_v2>
        symbols{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder accept_baseline{
        {}, {}, nodes, symbols, {}, {},
        LzssContextualAdaptiveHuffmanStreamAdmission::field_context_64k};
    auto result = accept_baseline.process(
        baseline_bytes, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.error.code, ErrorCode::none);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder reject_baseline{
        {}, {}, nodes, symbols, {}, {},
        LzssContextualAdaptiveHuffmanStreamAdmission::field_context_1m};
    result = reject_baseline.process(baseline_bytes, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);

    auto extended = baseline;
    extended.dictionary.window_size = UINT32_C(1) << 20;
    extended.dictionary_variant = 3;
    extended.context_variant = 2;
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        extended_bytes{};
    ASSERT_EQ(serialize_lzss_contextual_adaptive_huffman_stream_header(
                  extended, {}, extended_bytes),
              LzssContextualAdaptiveHuffmanStreamHeaderError::none);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder accept_any{
        {}, {}, nodes, symbols, {}, {}};
    result = accept_any.process(extended_bytes, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.error.code, ErrorCode::none);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder accept_extended{
        {}, {}, nodes, symbols, {}, {},
        LzssContextualAdaptiveHuffmanStreamAdmission::field_context_1m};
    result = accept_extended.process(extended_bytes, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.error.code, ErrorCode::none);
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder reject_extended{
        {}, {}, nodes, symbols, {}, {},
        LzssContextualAdaptiveHuffmanStreamAdmission::field_context_64k};
    result = reject_extended.process(extended_bytes, {}, end_flag());
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::malformed_stream);
}

TEST(LzssContextualAdaptiveHuffmanFrameStreamingDecoder,
     RejectsInvalidProfileAdmissionAtConstruction) {
    std::array<marc::entropy::internal::AdaptiveHuffmanNode,
               marc::entropy::internal::
                   contextual_adaptive_huffman_node_entries>
        nodes{};
    std::array<std::uint16_t,
               marc::entropy::internal::
                   contextual_adaptive_huffman_symbol_entries>
        symbols{};
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, {}, nodes, symbols, {}, {},
        static_cast<LzssContextualAdaptiveHuffmanStreamAdmission>(255)};
    auto result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    result = decoder.process({}, {}, 0);
    EXPECT_EQ(result.status, StreamStatus::error);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

#include "frame/lzss_contextual_tans_frame_streaming_encoder.hpp"

#include "frame/lzss_contextual_tans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_tans_profile.hpp"

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
using marc::entropy::internal::contextual_tans_encode_table_entries;

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

[[nodiscard]] LzssContextualTansStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    LzssContextualTansStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    return stream;
}

[[nodiscard]] std::array<
    std::byte, lzss_contextual_tans_stream_header_size> stream_header(
    const std::uint8_t frame_size,
    const std::uint8_t original_size) noexcept {
    std::array<std::byte, lzss_contextual_tans_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52}; bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02}; bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01}; bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02}; bytes[16] = std::byte{0x05};
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
    std::vector<std::byte> bytes(96);
    bytes[0] = std::byte{0x4d}; bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46}; bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40}; bytes[8] = static_cast<std::byte>(sequence);
    bytes[16] = std::byte{0x01}; bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02}; bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x02}; bytes[36] = std::byte{0x1e};
    constexpr std::array descriptor{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    std::ranges::copy(descriptor, bytes.begin() + 64);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    const auto header = stream_header(1, 2);
    const auto first = literal_frame(0);
    const auto second = literal_frame(1);
    std::vector<std::byte> expected(header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

[[nodiscard]] std::vector<std::uint16_t> tables() {
    return std::vector<std::uint16_t>(
        contextual_tans_encode_table_entries);
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

[[nodiscard]] std::vector<std::byte> encode_one_byte_chunks(
    LzssContextualTansFrameStreamingEncoder& encoder,
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

[[nodiscard]] std::vector<std::byte> decode_one_byte_chunks(
    LzssContextualTansFrameStreamingDecoder& decoder,
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
        const auto result = decoder.process(chunk, byte, flags);
        EXPECT_TRUE(marc::core::is_valid(result, chunk.size(), byte.size()));
        EXPECT_NE(result.status, StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) output.push_back(byte[0]);
        status = result.status;
    } while (status != StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    return output;
}

} // namespace

TEST(LzssContextualTansFrameStreamingEncoder, MatchesOneByteOracle) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    const auto expected = two_frame_stream();
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto table_storage = tables();
    std::array<std::byte, 96> frame{};
    LzssContextualTansFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, table_storage, frame};

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

TEST(LzssContextualTansFrameStreamingEncoder,
     EmitsFullFrameBeforeEndAndFlushKeepsPartialOpen) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    auto table_storage = tables();
    std::array<std::byte, 256> frame{};
    LzssContextualTansFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, table_storage, frame};
    std::vector<std::byte> output(1024);

    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced,
              lzss_contextual_tans_stream_header_size);
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

TEST(LzssContextualTansFrameStreamingEncoder,
     RetainsEndInputAcrossFinalAndEmptyDrain) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto table_storage = tables();
    std::array<std::byte, 96> frame{};
    LzssContextualTansFrameStreamingEncoder encoder{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    std::array<std::byte, lzss_contextual_tans_stream_header_size> header{};
    const auto header_result = encoder.process({}, header, 0);
    ASSERT_EQ(header_result.status, StreamStatus::progress);
    auto result = encoder.process(input, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 1U);
    std::array<std::byte, 96> encoded_frame{};
    result = encoder.process({}, encoded_frame, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded_frame.size());

    LzssContextualTansFrameStreamingEncoder empty{
        stream_config(1, 0), {}, {}, {}, {}, {}};
    result = empty.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    result = empty.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
}

TEST(LzssContextualTansFrameStreamingEncoder,
     ReportsCapacityLimitAndInputProtocolFailuresSticky) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto table_storage = tables();
    std::array<std::byte, 96> frame{};
    std::vector<std::byte> output(512);

    LzssContextualTansFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, table_storage, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_tokens.process({}, {}, 0).error.code,
              ErrorCode::out_of_memory);

    LzssContextualTansFrameStreamingEncoder short_tables{
        stream_config(1, 1), {}, raw, tokens,
        std::span<std::uint16_t>{table_storage}.first(
            table_storage.size() - 1),
        frame};
    result = short_tables.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    LzssContextualTansFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens, table_storage,
        std::span<std::byte>{frame}.first(frame.size() - 1)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 64;
    limits.max_block_size = 64;
    limits.max_internal_buffered_bytes =
        raw.size() + sizeof(LzssTypedToken)
        + table_storage.size() * sizeof(std::uint16_t) + frame.size() - 1;
    LzssContextualTansFrameStreamingEncoder limited{
        stream_config(1, 1), limits, raw, tokens, table_storage, frame};
    result = limited.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> premature_raw{};
    std::array<LzssTypedToken, 2> premature_tokens{};
    LzssContextualTansFrameStreamingEncoder premature{
        stream_config(2, 2), {}, premature_raw, premature_tokens,
        table_storage, frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    constexpr std::array excess{std::byte{'A'}, std::byte{'B'}};
    LzssContextualTansFrameStreamingEncoder too_much{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    result = too_much.process(excess, output, 0);
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
}

TEST(LzssContextualTansFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 1> shared{};
    auto shared_bytes = std::as_writable_bytes(std::span{shared});
    auto table_storage = tables();
    std::array<std::byte, 96> frame{};
    LzssContextualTansFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, shared_bytes.first(1), shared,
        table_storage, frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto table_bytes = std::as_writable_bytes(std::span{table_storage});
    LzssContextualTansFrameStreamingEncoder raw_table_overlap{
        stream_config(1, 1), {}, table_bytes.first(1), tokens,
        table_storage, frame};
    EXPECT_EQ(raw_table_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 96> raw_frame_shared{};
    LzssContextualTansFrameStreamingEncoder raw_frame_overlap{
        stream_config(1, 1), {},
        std::span<std::byte>{raw_frame_shared}.first(1), tokens,
        table_storage, raw_frame_shared};
    EXPECT_EQ(raw_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    auto token_bytes = std::as_writable_bytes(std::span{tokens});
    LzssContextualTansFrameStreamingEncoder token_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, table_storage, token_bytes};
    EXPECT_EQ(token_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder table_frame_overlap{
        stream_config(1, 1), {}, raw, tokens, table_storage,
        table_bytes.first(96)};
    EXPECT_EQ(table_frame_overlap.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder raw_output_alias{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    EXPECT_EQ(raw_output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);
    EXPECT_EQ(raw_output_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder token_output_alias{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    EXPECT_EQ(token_output_alias.process({}, token_bytes, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder table_output_alias{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    EXPECT_EQ(table_output_alias.process(
                  {}, table_bytes.first(1), 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder frame_output_alias{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    EXPECT_EQ(frame_output_alias.process({}, frame, 0).error.code,
              ErrorCode::invalid_argument);

    LzssContextualTansFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    auto result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              ErrorCode::unsupported);

    LzssContextualTansFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, table_storage, frame};
    result = reset.process(
        {}, {}, marc::core::flag_value(ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              ErrorCode::unsupported);
}

TEST(LzssContextualTansFrameStreamingEncoder,
     AcceptsSelectedOneMiBIdentityAfterLifecycleAdmission) {
    auto selected = stream_config(1, 1);
    selected.dictionary.window_size = UINT32_C(1) << 20;
    selected.dictionary_variant = 3;
    selected.context_variant = 2;
    selected.frequency_entry_count = 4550;
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    auto table_storage = tables();
    std::array<std::byte, 96> frame{};
    LzssContextualTansFrameStreamingEncoder encoder{
        selected, {}, raw, tokens, table_storage, frame};

    std::array<std::byte, lzss_contextual_tans_stream_header_size> output{};
    const auto result = encoder.process({}, output, 0);
    EXPECT_EQ(result.status, StreamStatus::progress);
    EXPECT_EQ(result.error.code, ErrorCode::none);
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(output[14], std::byte{0x03});
    EXPECT_EQ(output[98], std::byte{0x02});
}

TEST(LzssContextualTansFrameStreamingEncoder,
     OneMiBProfileStreamsExtendedDistanceWithOneByteBuffers) {
    constexpr std::size_t gap = 65536;
    std::vector<std::byte> raw(5 + gap + 5, std::byte{'Z'});
    constexpr std::array marker{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    std::ranges::copy(marker, raw.begin());
    std::ranges::copy(marker, raw.end() - marker.size());

    LzssContextualTansProfileConfig config{};
    config.original_size = raw.size();
    config.frame_size = static_cast<std::uint32_t>(raw.size());
    config.dictionary.window_size = UINT32_C(1) << 20;
    config.variant = LzssContextualTansProfileVariant::field_context_1m;
    LzssContextualTansStreamHeader stream{};
    LzssContextualTansEncoderWorkspaceRequirements encoder_requirements{};
    ASSERT_EQ(make_lzss_contextual_tans_profile(
                  config, {}, stream, encoder_requirements),
              LzssContextualTansProfileError::none);
    std::vector<std::byte> frame_input(
        encoder_requirements.frame_input_bytes);
    std::vector<std::byte> frame_encoded(
        encoder_requirements.frame_encoded_bytes);
    AlignedWorkspace encoder_owner(encoder_requirements.views_bytes);
    LzssContextualTansEncoderViews encoder_views{};
    ASSERT_EQ(partition_lzss_contextual_tans_encoder_views(
                  encoder_requirements,
                  encoder_owner.bytes(encoder_requirements.views_bytes),
                  encoder_views),
              LzssContextualTansWorkspaceError::none);
    LzssContextualTansFrameStreamingEncoder encoder{
        stream, {}, frame_input, encoder_views.tokens,
        encoder_views.tables, encoder_views.match_finder, frame_encoded};
    const auto encoded = encode_one_byte_chunks(encoder, raw);
    ASSERT_EQ(encoded[14], std::byte{0x03});
    ASSERT_EQ(encoded[98], std::byte{0x02});
    ASSERT_TRUE(std::ranges::any_of(
        encoder_views.tokens, [](const LzssTypedToken& token) {
            return token.kind
                    == marc::dictionary::internal::LzssTypedTokenKind::match
                && token.distance > 65536;
        }));

    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = raw.size();
    limits.max_block_size = raw.size();
    limits.max_compressed_payload_size = 1U << 20;
    LzssContextualTansDecoderWorkspaceRequirements decoder_requirements{};
    ASSERT_EQ(calculate_lzss_contextual_tans_decoder_workspace(
                  limits, decoder_requirements,
                  LzssContextualTansProfileVariant::field_context_1m),
              LzssContextualTansProfileError::none);
    std::vector<std::byte> decode_encoded(
        decoder_requirements.frame_encoded_bytes);
    std::vector<std::byte> decode_raw(
        decoder_requirements.frame_decoded_bytes);
    AlignedWorkspace decoder_owner(decoder_requirements.views_bytes);
    LzssContextualTansDecoderViews decoder_views{};
    ASSERT_EQ(partition_lzss_contextual_tans_decoder_views(
                  decoder_requirements,
                  decoder_owner.bytes(decoder_requirements.views_bytes),
                  decoder_views),
              LzssContextualTansWorkspaceError::none);
    LzssContextualTansFrameStreamingDecoder decoder{
        limits, decode_encoded, decoder_views.tables, decoder_views.tokens,
        decode_raw, LzssContextualTansStreamAdmission::field_context_1m};
    EXPECT_EQ(decode_one_byte_chunks(decoder, encoded), raw);
}

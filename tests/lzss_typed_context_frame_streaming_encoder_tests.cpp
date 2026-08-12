#include "frame/lzss_typed_context_frame_streaming_encoder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::context::internal::ModeledOperation;
using marc::core::ErrorCode;
using marc::core::ProcessFlags;
using marc::core::StreamStatus;
using marc::dictionary::internal::LzssTypedToken;

[[nodiscard]] TypedContextStreamHeader stream_config(
    const std::uint32_t frame_size,
    const std::uint64_t original_size) noexcept {
    TypedContextStreamHeader stream{};
    stream.frame_size = frame_size;
    stream.original_size = original_size;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    return stream;
}

[[nodiscard]] constexpr std::array<std::byte, 112> stream_header(
    const std::uint8_t frame_size,
    const std::uint8_t original_size) noexcept {
    std::array<std::byte, 112> encoded{};
    encoded[0] = std::byte{0x4D};
    encoded[1] = std::byte{0x41};
    encoded[2] = std::byte{0x52};
    encoded[3] = std::byte{0x43};
    encoded[4] = std::byte{0x02};
    encoded[8] = std::byte{0x40};
    encoded[10] = std::byte{0x01};
    encoded[12] = std::byte{0x02};
    encoded[14] = std::byte{0x02};
    encoded[16] = std::byte{0x03};
    encoded[18] = std::byte{0x02};
    encoded[20] = static_cast<std::byte>(frame_size);
    encoded[28] = std::byte{0x10};
    encoded[32] = std::byte{0x10};
    encoded[40] = static_cast<std::byte>(original_size);
    encoded[48] = std::byte{0x10};
    encoded[66] = std::byte{0x01};
    encoded[68] = std::byte{0x05};
    encoded[72] = std::byte{0x02};
    encoded[73] = std::byte{0x01};
    encoded[81] = std::byte{0x80};
    encoded[84] = std::byte{0x1F};
    encoded[96] = std::byte{0x01};
    encoded[98] = std::byte{0x01};
    return encoded;
}

[[nodiscard]] constexpr std::array<std::byte, 86> literal_frame(
    const std::uint8_t sequence) noexcept {
    std::array<std::byte, 86> encoded{};
    encoded[0] = std::byte{0x4D};
    encoded[1] = std::byte{0x52};
    encoded[2] = std::byte{0x46};
    encoded[3] = std::byte{0x32};
    encoded[4] = std::byte{0x40};
    encoded[8] = static_cast<std::byte>(sequence);
    encoded[16] = std::byte{0x01};
    encoded[20] = std::byte{0x01};
    encoded[24] = std::byte{0x02};
    encoded[28] = std::byte{0x02};
    encoded[32] = std::byte{0x06};
    encoded[36] = std::byte{0x10};
    encoded[64] = std::byte{0x02};
    encoded[68] = std::byte{0x06};
    encoded[72] = std::byte{0x1F};
    encoded[80] = std::byte{0x00};
    encoded[81] = std::byte{0x20};
    encoded[82] = std::byte{0x7F};
    encoded[83] = std::byte{0xFF};
    encoded[84] = std::byte{0xBF};
    encoded[85] = std::byte{0x00};
    return encoded;
}

[[nodiscard]] std::vector<std::byte> two_frame_stream() {
    constexpr auto header = stream_header(1, 2);
    constexpr auto first = literal_frame(0);
    constexpr auto second = literal_frame(1);
    std::vector<std::byte> expected;
    expected.insert(expected.end(), header.begin(), header.end());
    expected.insert(expected.end(), first.begin(), first.end());
    expected.insert(expected.end(), second.begin(), second.end());
    return expected;
}

[[nodiscard]] constexpr std::uint32_t end_flag() noexcept {
    return marc::core::flag_value(ProcessFlags::end_input);
}

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

} // namespace

TEST(LzssTypedContextFrameStreamingEncoder, MatchesOneByteOracle) {
    constexpr std::array input{std::byte{'A'}, std::byte{'A'}};
    const auto expected = two_frame_stream();
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<ModeledOperation, 2> operations{};
    std::array<std::byte, 86> frame{};
    LzssTypedContextFrameStreamingEncoder encoder{
        stream_config(1, input.size()), {}, raw, tokens, operations, {}, frame};

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

TEST(LzssTypedContextFrameStreamingEncoder,
     EmitsFullFrameBeforeEndAndFlushKeepsPartialOpen) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};
    std::array<std::byte, 2> raw{};
    std::array<LzssTypedToken, 2> tokens{};
    std::array<ModeledOperation, 4> operations{};
    std::array<std::byte, 128> frame{};
    LzssTypedContextFrameStreamingEncoder encoder{
        stream_config(2, input.size()), {}, raw, tokens, operations, {}, frame};
    std::array<std::byte, 512> output{};

    const auto first = encoder.process(
        std::span<const std::byte>{input}.first(1), output,
        marc::core::flag_value(ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced, typed_context_stream_header_size);
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

TEST(LzssTypedContextFrameStreamingEncoder,
     RetainsEndInputAcrossFinalAndEmptyDrain) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<ModeledOperation, 2> operations{};
    std::array<std::byte, 86> frame{};
    LzssTypedContextFrameStreamingEncoder encoder{
        stream_config(1, 1), {}, raw, tokens, operations, {}, frame};
    std::array<std::byte, typed_context_stream_header_size> header{};
    const auto header_result = encoder.process({}, header, 0);
    ASSERT_EQ(header_result.status, StreamStatus::progress);
    ASSERT_EQ(header_result.output_produced, header.size());
    auto result = encoder.process(input, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, 1U);
    std::array<std::byte, 86> encoded_frame{};
    result = encoder.process({}, encoded_frame, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, encoded_frame.size());

    LzssTypedContextFrameStreamingEncoder empty{
        stream_config(1, 0), {}, {}, {}, {}, {}, {}};
    result = empty.process({}, {}, end_flag());
    ASSERT_EQ(result.status, StreamStatus::need_output);
    result = empty.process({}, header, 0);
    EXPECT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, header.size());
}

TEST(LzssTypedContextFrameStreamingEncoder,
     ReportsCapacityLimitAndPrematureEnd) {
    constexpr std::array input{std::byte{'A'}};
    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<ModeledOperation, 2> operations{};
    std::array<std::byte, 86> frame{};
    std::array<std::byte, 256> output{};

    LzssTypedContextFrameStreamingEncoder short_tokens{
        stream_config(1, 1), {}, raw, {}, operations, {}, frame};
    auto result = short_tokens.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    LzssTypedContextFrameStreamingEncoder short_operations{
        stream_config(1, 1), {}, raw, tokens,
        std::span<ModeledOperation>{operations}.first(1), {}, frame};
    result = short_operations.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    LzssTypedContextFrameStreamingEncoder short_frame{
        stream_config(1, 1), {}, raw, tokens, operations, {},
        std::span<std::byte>{frame}.first(85)};
    result = short_frame.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::out_of_memory);

    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 5;
    LzssTypedContextFrameStreamingEncoder limited{
        stream_config(1, 1), limits, raw, tokens, operations, {}, frame};
    result = limited.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::limit_exceeded);

    std::array<std::byte, 2> premature_raw{};
    std::array<LzssTypedToken, 2> premature_tokens{};
    std::array<ModeledOperation, 4> premature_operations{};
    std::array<std::byte, 128> premature_frame{};
    LzssTypedContextFrameStreamingEncoder premature{
        stream_config(2, 2), {},
        premature_raw, premature_tokens, premature_operations, {},
        premature_frame};
    result = premature.process(input, output, end_flag());
    EXPECT_EQ(result.error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
}

TEST(LzssTypedContextFrameStreamingEncoder,
     RejectsAliasesAndUnsupportedFlagsSticky) {
    std::array<LzssTypedToken, 8> shared{};
    auto shared_bytes = std::as_writable_bytes(std::span{shared});
    std::array<ModeledOperation, 2> operations{};
    std::array<std::byte, 86> frame{};
    LzssTypedContextFrameStreamingEncoder overlapping{
        stream_config(1, 1), {}, shared_bytes.first(1),
        std::span<LzssTypedToken>{shared}.first(1), operations, {}, frame};
    EXPECT_EQ(overlapping.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);

    std::array<std::byte, 1> raw{};
    std::array<LzssTypedToken, 1> tokens{};
    LzssTypedContextFrameStreamingEncoder output_alias{
        stream_config(1, 1), {}, raw, tokens, operations, {}, frame};
    EXPECT_EQ(output_alias.process({}, raw, 0).error.code,
              ErrorCode::invalid_argument);

    LzssTypedContextFrameStreamingEncoder unknown{
        stream_config(1, 1), {}, raw, tokens, operations, {}, frame};
    auto result = unknown.process({}, {}, UINT32_C(1) << 31);
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
    EXPECT_EQ(unknown.process({}, {}, 0).error.code,
              ErrorCode::unsupported);

    LzssTypedContextFrameStreamingEncoder reset{
        stream_config(1, 1), {}, raw, tokens, operations, {}, frame};
    result = reset.process(
        {}, {}, marc::core::flag_value(ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, ErrorCode::unsupported);
}

TEST(LzssTypedContextFrameStreamingEncoder,
     HashChainMatchesExhaustiveStreamAndRequiresWorkspace) {
    constexpr std::array input{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'},
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'3'}};
    const auto stream = stream_config(input.size(), input.size());
    std::array<std::byte, input.size()> raw{};
    std::array<LzssTypedToken, input.size()> tokens{};
    std::array<ModeledOperation, input.size() * 2> operations{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(input.size(), {}, {});
    ASSERT_EQ(required.error,
              marc::dictionary::internal::LzssHashChainError::none);
    ASSERT_GT(required.workspace_size, 0U);
    AlignedWorkspace owner(required.workspace_size);
    auto finder = owner.bytes(required.workspace_size);

    const auto frame_plan = plan_lzss_typed_context_frame(
        stream, {}, 0, 0, input, tokens, operations);
    ASSERT_EQ(frame_plan.error, LzssTypedContextFrameEncodeError::none);
    std::vector<std::byte> frame(frame_plan.serialized_size);
    ASSERT_EQ(encode_lzss_typed_context_frame(
                  stream, {}, 0, 0, input, tokens, operations, frame).error,
              LzssTypedContextFrameEncodeError::none);
    std::array<std::byte, typed_context_stream_header_size> header{};
    ASSERT_EQ(serialize_typed_context_stream_header(stream, {}, header),
              TypedContextStreamHeaderError::none);
    std::vector<std::byte> expected(
        typed_context_stream_header_size + frame.size());
    std::ranges::copy(header, expected.begin());
    std::ranges::copy(
        frame, expected.begin() + typed_context_stream_header_size);

    std::vector<std::byte> serialized(frame_plan.serialized_size);
    LzssTypedContextFrameStreamingEncoder encoder{
        stream, {}, raw, tokens, operations, finder, serialized};
    std::vector<std::byte> actual(expected.size());
    const auto result = encoder.process(input, actual, end_flag());
    ASSERT_EQ(result.status, StreamStatus::end_of_stream);
    EXPECT_EQ(result.input_consumed, input.size());
    EXPECT_EQ(result.output_produced, expected.size());
    EXPECT_EQ(actual, expected);

    LzssTypedContextFrameStreamingEncoder short_finder{
        stream, {}, raw, tokens, operations,
        finder.first(finder.size() - 1), serialized};
    std::ranges::fill(actual, std::byte{0xCC});
    const auto short_result = short_finder.process(input, actual, end_flag());
    EXPECT_EQ(short_result.status, StreamStatus::error);
    EXPECT_EQ(short_result.error.code, ErrorCode::out_of_memory);
    EXPECT_EQ(short_result.input_consumed, input.size());
    EXPECT_EQ(short_result.output_produced,
              typed_context_stream_header_size);

    LzssTypedContextFrameStreamingEncoder finder_alias{
        stream, {}, raw, tokens, operations,
        std::span<std::byte>{raw}, serialized};
    EXPECT_EQ(finder_alias.process({}, {}, 0).error.code,
              ErrorCode::invalid_argument);
}

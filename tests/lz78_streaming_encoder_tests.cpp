#include "dictionary/lz78_decoder.hpp"
#include "dictionary/lz78_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    for (const char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

TEST(Lz78StreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto raw = bytes("AABABCABC");
    std::array<Lz78EncoderEntry, 9> reference_workspace{};
    const auto plan = plan_lz78_token_stream(
        raw, {}, {}, reference_workspace);
    std::vector<std::byte> reference(plan.output_size);
    ASSERT_EQ(encode_lz78_token_stream(
                  raw, {}, {}, reference_workspace, reference).error,
              Lz78EncodeError::none);

    std::array<std::byte, 9> raw_storage{};
    std::array<std::byte, 72> encoded_storage{};
    std::array<Lz78EncoderEntry, 9> dictionary{};
    Lz78StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage, dictionary};
    std::vector<std::byte> actual;
    std::size_t position{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, raw.size() - position);
        const auto chunk = std::span<const std::byte>{raw}.subspan(position,
                                                                  count);
        const auto flags = position + count == raw.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0) actual.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, raw.size());
    EXPECT_EQ(actual, reference);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(Lz78StreamingEncoder, FullFrameCanDrainBeforeEndInput) {
    const auto raw = bytes("ABAB");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 24> encoded_storage{};
    std::array<Lz78EncoderEntry, 4> dictionary{};
    Lz78StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage, dictionary};
    std::array<std::byte, 24> output{};
    auto result = encoder.process(raw, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, output.size());
    result = encoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(Lz78StreamingEncoder, PreservesEndInputWhileDraining) {
    const auto raw = bytes("AABABCABC");
    std::array<std::byte, 9> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    std::array<Lz78EncoderEntry, 9> dictionary{};
    Lz78StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage, dictionary};
    std::array<std::byte, 32> output{};
    auto result = encoder.process(
        raw, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, raw.size());
    std::size_t produced = result.output_produced;
    result = encoder.process({}, std::span<std::byte>{output}.subspan(produced),
                             0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    produced += result.output_produced;
    EXPECT_EQ(produced, output.size());

    std::array<Lz78PhraseEntry, 9> decode_dictionary{};
    std::array<std::byte, 9> decoded{};
    EXPECT_EQ(decode_lz78_token_stream(
                  output, {}, raw.size(), {}, decode_dictionary, decoded).error,
              Lz78DecodeError::none);
    EXPECT_EQ(std::vector(decoded.begin(), decoded.end()), raw);
}

TEST(Lz78StreamingEncoder, FlushDoesNotClosePartialFrame) {
    const auto raw = bytes("AABABCABC");
    std::array<std::byte, 9> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    std::array<Lz78EncoderEntry, 9> dictionary{};
    Lz78StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage, dictionary};
    auto result = encoder.process(
        std::span<const std::byte>{raw}.first(4), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, 0U);
    result = encoder.process(
        std::span<const std::byte>{raw}.subspan(4), encoded_storage,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(Lz78StreamingEncoder, PreservesDictionaryFreeze) {
    const auto raw = bytes("AAA");
    Lz78Parameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    std::array<std::byte, 3> raw_storage{};
    std::array<std::byte, 16> encoded_storage{};
    std::array<Lz78EncoderEntry, 1> dictionary{};
    Lz78StreamingEncoder encoder{parameters, raw.size(), limits, raw_storage,
                                 encoded_storage, dictionary};
    const auto result = encoder.process(
        raw, encoded_storage,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, 16U);
    EXPECT_EQ(encoded_storage[8], std::byte{0});
    EXPECT_EQ(encoded_storage[12], std::byte{1});
}

TEST(Lz78StreamingEncoder, ReportsPrematureTrailingAndWorkspaceFailures) {
    const auto raw = bytes("ABAB");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    std::array<Lz78EncoderEntry, 4> dictionary{};
    Lz78StreamingEncoder premature{{}, raw.size(), {}, raw_storage,
                                   encoded_storage, dictionary};
    auto result = premature.process(
        std::span<const std::byte>{raw}.first(3), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    Lz78StreamingEncoder trailing{{}, 3, {}, raw_storage, encoded_storage,
                                  dictionary};
    result = trailing.process(raw, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 3U);

    std::array<std::byte, 23> short_encoded{};
    Lz78StreamingEncoder short_output{{}, raw.size(), {}, raw_storage,
                                      short_encoded, dictionary};
    result = short_output.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 3> short_raw{};
    Lz78StreamingEncoder short_input{{}, raw.size(), {}, short_raw,
                                     encoded_storage, dictionary};
    result = short_input.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<Lz78EncoderEntry, 3> short_dictionary{};
    Lz78StreamingEncoder short_table{{}, raw.size(), {}, raw_storage,
                                     encoded_storage, short_dictionary};
    result = short_table.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        raw.size() + sizeof(Lz78EncoderEntry) * raw.size() + 23;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    Lz78StreamingEncoder aggregate_limit{{}, raw.size(), limits, raw_storage,
                                         encoded_storage, dictionary};
    result = aggregate_limit.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz78StreamingEncoder, HandlesEmptyAndRejectsReset) {
    Lz78StreamingEncoder empty{{}, 0, {}, {}, {}, {}};
    auto result = empty.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 1> raw_storage{};
    std::array<std::byte, 8> encoded_storage{};
    std::array<Lz78EncoderEntry, 1> dictionary{};
    Lz78StreamingEncoder reset{{}, 1, {}, raw_storage, encoded_storage,
                               dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

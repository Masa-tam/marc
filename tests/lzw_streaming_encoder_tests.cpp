#include "dictionary/lzw_decoder.hpp"
#include "dictionary/lzw_streaming_encoder.hpp"

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

std::vector<std::byte> boundary_input() {
    std::vector<std::byte> input(2048);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<std::byte>(
            (index * 37 + index / 7) & 0xffU);
    }
    return input;
}

std::vector<std::byte> reference(
    const std::span<const std::byte> raw,
    const LzwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzwEncoderEntry> workspace(
        lzw_encoder_workspace_entries(raw.size(), parameters));
    const auto plan = plan_lzw_code_stream(
        raw, parameters, limits, workspace);
    EXPECT_EQ(plan.error, LzwEncodeError::none);
    std::vector<std::byte> output(plan.output_size);
    EXPECT_EQ(encode_lzw_code_stream(
                  raw, parameters, limits, workspace, output).error,
              LzwEncodeError::none);
    return output;
}

TEST(LzwStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto raw = bytes("ABABABA");
    const auto expected = reference(raw);
    std::array<std::byte, 7> raw_storage{};
    std::array<std::byte, 5> encoded_storage{};
    std::array<LzwEncoderEntry, 6> dictionary{};
    LzwStreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
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
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzwStreamingEncoder, FullFrameCanDrainBeforeEndInput) {
    const auto raw = bytes("AAA");
    std::array<std::byte, 3> raw_storage{};
    std::array<std::byte, 3> encoded_storage{};
    std::array<LzwEncoderEntry, 2> dictionary{};
    LzwStreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                encoded_storage, dictionary};
    std::array<std::byte, 3> output{};
    auto result = encoder.process(raw, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, output.size());
    result = encoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(LzwStreamingEncoder, PreservesEndInputWhileDraining) {
    const auto raw = bytes("ABABABA");
    std::array<std::byte, 7> raw_storage{};
    std::array<std::byte, 5> encoded_storage{};
    std::array<LzwEncoderEntry, 6> dictionary{};
    LzwStreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                encoded_storage, dictionary};
    std::array<std::byte, 5> output{};
    auto result = encoder.process(
        raw, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, raw.size());
    std::size_t produced = result.output_produced;
    result = encoder.process(
        {}, std::span<std::byte>{output}.subspan(produced), 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    produced += result.output_produced;
    EXPECT_EQ(produced, output.size());

    std::array<LzwPhraseEntry, 6> decode_dictionary{};
    std::array<std::byte, 7> decoded{};
    EXPECT_EQ(decode_lzw_code_stream(
                  output, {}, raw.size(), {}, decode_dictionary, decoded).error,
              LzwDecodeError::none);
    EXPECT_EQ(std::vector(decoded.begin(), decoded.end()), raw);
}

TEST(LzwStreamingEncoder, FlushDoesNotClosePartialFrame) {
    const auto raw = bytes("ABABABA");
    std::array<std::byte, 7> raw_storage{};
    std::array<std::byte, 5> encoded_storage{};
    std::array<LzwEncoderEntry, 6> dictionary{};
    LzwStreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                encoded_storage, dictionary};
    auto result = encoder.process(
        std::span<const std::byte>{raw}.first(3), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, 0U);
    result = encoder.process(
        std::span<const std::byte>{raw}.subspan(3), encoded_storage,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(std::vector(encoded_storage.begin(), encoded_storage.end()),
              reference(raw));
}

TEST(LzwStreamingEncoder, MatchesReferenceAcrossWidthBoundaryAndFreeze) {
    const auto raw = boundary_input();
    const auto expected = reference(raw);
    std::vector<std::byte> raw_storage(raw.size());
    std::vector<std::byte> encoded_storage(expected.size());
    std::vector<LzwEncoderEntry> dictionary(raw.size() - 1);
    LzwStreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                encoded_storage, dictionary};
    std::vector<std::byte> output(expected.size());
    auto result = encoder.process(
        raw, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(output, expected);

    LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 256;
    const auto frozen_expected = reference(raw, parameters, limits);
    encoded_storage.resize(frozen_expected.size());
    output.resize(frozen_expected.size());
    dictionary.resize(256);
    LzwStreamingEncoder frozen{parameters, raw.size(), limits, raw_storage,
                               encoded_storage, dictionary};
    result = frozen.process(
        raw, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(output, frozen_expected);
}

TEST(LzwStreamingEncoder, ReportsPrematureTrailingAndWorkspaceFailures) {
    const auto raw = bytes("ABABABA");
    std::array<std::byte, 7> raw_storage{};
    std::array<std::byte, 5> encoded_storage{};
    std::array<LzwEncoderEntry, 6> dictionary{};
    LzwStreamingEncoder premature{{}, raw.size(), {}, raw_storage,
                                  encoded_storage, dictionary};
    auto result = premature.process(
        std::span<const std::byte>{raw}.first(raw.size() - 1), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    LzwStreamingEncoder trailing{{}, raw.size() - 1, {}, raw_storage,
                                 encoded_storage, dictionary};
    result = trailing.process(raw, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, raw.size() - 1);

    std::array<std::byte, 4> short_encoded{};
    LzwStreamingEncoder short_output{{}, raw.size(), {}, raw_storage,
                                     short_encoded, dictionary};
    result = short_output.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 6> short_raw{};
    LzwStreamingEncoder short_input{{}, raw.size(), {}, short_raw,
                                    encoded_storage, dictionary};
    result = short_input.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<LzwEncoderEntry, 5> short_dictionary{};
    LzwStreamingEncoder short_table{{}, raw.size(), {}, raw_storage,
                                    encoded_storage, short_dictionary};
    result = short_table.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        raw.size() + sizeof(LzwEncoderEntry) * dictionary.size()
        + encoded_storage.size() - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzwStreamingEncoder aggregate_limit{{}, raw.size(), limits, raw_storage,
                                         encoded_storage, dictionary};
    result = aggregate_limit.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzwStreamingEncoder, HandlesEmptyAndRejectsReset) {
    LzwStreamingEncoder empty{{}, 0, {}, {}, {}, {}};
    auto result = empty.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(empty.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 1> raw_storage{};
    std::array<std::byte, 2> encoded_storage{};
    LzwStreamingEncoder reset{{}, 1, {}, raw_storage, encoded_storage, {}};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

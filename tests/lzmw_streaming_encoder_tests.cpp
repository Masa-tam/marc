#include "dictionary/lzmw_decoder.hpp"
#include "dictionary/lzmw_streaming_encoder.hpp"

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

std::vector<std::byte> reference_encode(
    const std::span<const std::byte> raw,
    const LzmwParameters parameters = {},
    const marc::core::DecoderLimits limits = {}) {
    std::vector<LzmwEncoderEntry> dictionary(
        lzmw_encoder_workspace_entries(raw.size(), parameters));
    const auto plan = plan_lzmw_token_stream(
        raw, parameters, limits, dictionary);
    EXPECT_EQ(plan.error, LzmwEncodeError::none);
    std::vector<std::byte> result(plan.output_size);
    EXPECT_EQ(encode_lzmw_token_stream(
                  raw, parameters, limits, dictionary, result).error,
              LzmwEncodeError::none);
    return result;
}

struct EncoderStorage {
    EncoderStorage(const std::size_t raw_size,
                   const LzmwParameters parameters = {}) {
        requirements = lzmw_streaming_encoder_workspace_requirements(
            raw_size, parameters);
        raw.resize(requirements.raw_bytes);
        encoded.resize(requirements.encoded_bytes);
        dictionary.resize(requirements.dictionary_entries);
    }

    LzmwStreamingEncoderWorkspaceRequirements requirements{};
    std::vector<std::byte> raw;
    std::vector<std::byte> encoded;
    std::vector<LzmwEncoderEntry> dictionary;
};

TEST(LzmwStreamingEncoder, ReportsBoundedWorkspaceRequirements) {
    const auto empty = lzmw_streaming_encoder_workspace_requirements(0, {});
    EXPECT_TRUE(empty.supported);
    EXPECT_EQ(empty.raw_bytes, 0U);
    EXPECT_EQ(empty.encoded_bytes, 0U);
    EXPECT_EQ(empty.dictionary_entries, 0U);

    const auto one = lzmw_streaming_encoder_workspace_requirements(1, {});
    EXPECT_TRUE(one.supported);
    EXPECT_EQ(one.raw_bytes, 1U);
    EXPECT_EQ(one.encoded_bytes, 4U);
    EXPECT_EQ(one.dictionary_entries, 0U);

    LzmwParameters frozen{};
    frozen.maximum_entries = 1;
    const auto ten = lzmw_streaming_encoder_workspace_requirements(10, frozen);
    EXPECT_TRUE(ten.supported);
    EXPECT_EQ(ten.raw_bytes, 10U);
    EXPECT_EQ(ten.encoded_bytes, 40U);
    EXPECT_EQ(ten.dictionary_entries, 1U);
    EXPECT_FALSE(lzmw_streaming_encoder_workspace_requirements(
                     UINT64_MAX, {}).supported);
}

TEST(LzmwStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto raw = bytes("abbaababaaba");
    const auto reference = reference_encode(raw);
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder encoder{{}, raw.size(), {}, storage.raw,
                                  storage.encoded, storage.dictionary};
    std::vector<std::byte> actual;
    std::array<std::byte, 1> output{};
    std::size_t position{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, raw.size() - position);
        const auto chunk = std::span<const std::byte>{raw}.subspan(
            position, count);
        const auto flags = position + count == raw.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
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

TEST(LzmwStreamingEncoder, FullFrameCanDrainBeforeEndInput) {
    const auto raw = bytes("ABAB");
    const auto reference = reference_encode(raw);
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder encoder{{}, raw.size(), {}, storage.raw,
                                  storage.encoded, storage.dictionary};
    std::vector<std::byte> output(reference.size());
    auto result = encoder.process(raw, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(output, reference);
    result = encoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwStreamingEncoder, PreservesEndInputWhileDraining) {
    const auto raw = bytes("ABABABABAB");
    const auto reference = reference_encode(raw);
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder encoder{{}, raw.size(), {}, storage.raw,
                                  storage.encoded, storage.dictionary};
    std::vector<std::byte> output(reference.size());
    auto result = encoder.process(
        raw, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, raw.size());
    std::size_t produced = result.output_produced;
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        result = encoder.process(
            {}, std::span<std::byte>{output}.subspan(produced), 0);
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        produced += result.output_produced;
    }
    EXPECT_EQ(produced, reference.size());
    EXPECT_EQ(output, reference);
}

TEST(LzmwStreamingEncoder, RejectsNewInputBeforeFurtherDrain) {
    const auto raw = bytes("ABABAB");
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder encoder{{}, raw.size(), {}, storage.raw,
                                  storage.encoded, storage.dictionary};
    std::array<std::byte, 1> output{};
    auto result = encoder.process(raw, output, 0);
    ASSERT_EQ(result.status, marc::core::StreamStatus::need_output);
    const std::array trailing{std::byte{0}};
    output[0] = std::byte{0xcc};
    result = encoder.process(trailing, output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_EQ(output[0], std::byte{0xcc});
}

TEST(LzmwStreamingEncoder, FlushDoesNotClosePartialFrame) {
    const auto raw = bytes("abbaababaaba");
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder encoder{{}, raw.size(), {}, storage.raw,
                                  storage.encoded, storage.dictionary};
    auto result = encoder.process(
        std::span<const std::byte>{raw}.first(4), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, 0U);
    std::vector<std::byte> output(reference_encode(raw).size());
    result = encoder.process(
        std::span<const std::byte>{raw}.subspan(4), output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(output, reference_encode(raw));
}

TEST(LzmwStreamingEncoder, PreservesDictionaryFreeze) {
    const auto raw = bytes("ABABABABAB");
    LzmwParameters parameters{};
    parameters.maximum_entries = 1;
    marc::core::DecoderLimits limits{};
    limits.max_dictionary_entries = 1;
    const auto reference = reference_encode(raw, parameters, limits);
    EncoderStorage storage{raw.size(), parameters};
    LzmwStreamingEncoder encoder{parameters, raw.size(), limits, storage.raw,
                                  storage.encoded, storage.dictionary};
    std::vector<std::byte> output(reference.size());
    const auto result = encoder.process(
        raw, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(output, reference);
}

TEST(LzmwStreamingEncoder, ReportsPrematureTrailingAndWorkspaceFailures) {
    const auto raw = bytes("ABAB");
    EncoderStorage storage{raw.size()};
    LzmwStreamingEncoder premature{{}, raw.size(), {}, storage.raw,
                                    storage.encoded, storage.dictionary};
    auto result = premature.process(
        std::span<const std::byte>{raw}.first(3), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    LzmwStreamingEncoder trailing{{}, 3, {}, storage.raw, storage.encoded,
                                   storage.dictionary};
    result = trailing.process(raw, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 3U);

    LzmwStreamingEncoder short_raw{{}, raw.size(), {}, {}, storage.encoded,
                                    storage.dictionary};
    result = short_raw.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
    LzmwStreamingEncoder short_encoded{{}, raw.size(), {}, storage.raw, {},
                                        storage.dictionary};
    result = short_encoded.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
    LzmwStreamingEncoder short_dictionary{{}, raw.size(), {}, storage.raw,
                                           storage.encoded, {}};
    result = short_dictionary.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        storage.requirements.raw_bytes + storage.requirements.encoded_bytes
        + storage.requirements.dictionary_entries * sizeof(LzmwEncoderEntry)
        - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    LzmwStreamingEncoder aggregate_limit{
        {}, raw.size(), limits, storage.raw, storage.encoded,
        storage.dictionary};
    result = aggregate_limit.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzmwStreamingEncoder, HandlesEmptyAndRejectsReset) {
    LzmwStreamingEncoder empty{{}, 0, {}, {}, {}, {}};
    auto result = empty.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    EncoderStorage storage{1};
    LzmwStreamingEncoder reset{{}, 1, {}, storage.raw, storage.encoded,
                                storage.dictionary};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

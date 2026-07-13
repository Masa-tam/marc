#include "dictionary/lz77_decoder.hpp"
#include "dictionary/lz77_streaming_encoder.hpp"

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

TEST(Lz77StreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto raw = bytes("ABCABCXAAAA");
    const auto plan = plan_lz77_token_stream(raw, {}, {});
    std::vector<std::byte> reference(plan.output_size);
    ASSERT_EQ(encode_lz77_token_stream(raw, {}, {}, reference).error,
              Lz77EncodeError::none);
    std::array<std::byte, 11> raw_storage{};
    std::array<std::byte, 176> encoded_storage{};
    Lz77StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage};
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

TEST(Lz77StreamingEncoder, FullFrameCanDrainBeforeEndInput) {
    const auto raw = bytes("AAAA");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    Lz77StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage};
    std::array<std::byte, 32> output{};
    auto result = encoder.process(raw, output, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, output.size());
    result = encoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(Lz77StreamingEncoder, PreservesEndInputWhileDraining) {
    const auto raw = bytes("AAAA");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    Lz77StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage};
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
}

TEST(Lz77StreamingEncoder, FlushDoesNotClosePartialFrame) {
    const auto raw = bytes("ABCD");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 64> encoded_storage{};
    Lz77StreamingEncoder encoder{{}, raw.size(), {}, raw_storage,
                                 encoded_storage};
    auto result = encoder.process(
        std::span<const std::byte>{raw}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.output_produced, 0U);
    result = encoder.process(
        std::span<const std::byte>{raw}.subspan(2), encoded_storage,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(Lz77StreamingEncoder, ReportsPrematureEndTrailingInputAndWorkspace) {
    const auto raw = bytes("AAAA");
    std::array<std::byte, 4> raw_storage{};
    std::array<std::byte, 32> encoded_storage{};
    Lz77StreamingEncoder premature{{}, raw.size(), {}, raw_storage,
                                   encoded_storage};
    auto result = premature.process(
        std::span<const std::byte>{raw}.first(3), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);

    Lz77StreamingEncoder trailing{{}, 3, {}, raw_storage, encoded_storage};
    result = trailing.process(raw, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 3U);

    std::array<std::byte, 31> short_encoded{};
    Lz77StreamingEncoder short_output{{}, raw.size(), {}, raw_storage,
                                      short_encoded};
    result = short_output.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 3> short_raw{};
    Lz77StreamingEncoder short_input{{}, raw.size(), {}, short_raw,
                                     encoded_storage};
    result = short_input.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 35;
    Lz77StreamingEncoder aggregate_limit{{}, raw.size(), limits, raw_storage,
                                         encoded_storage};
    result = aggregate_limit.process(
        raw, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(Lz77StreamingEncoder, HandlesEmptyAndRejectsReset) {
    Lz77StreamingEncoder empty{{}, 0, {}, {}, {}};
    auto result = empty.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);

    std::array<std::byte, 1> raw_storage{};
    std::array<std::byte, 16> encoded_storage{};
    Lz77StreamingEncoder reset{{}, 1, {}, raw_storage, encoded_storage};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

} // namespace

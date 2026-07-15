#include "dictionary/lzmw_encoder.hpp"
#include "dictionary/lzmw_streaming_decoder.hpp"

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

std::vector<std::byte> encoded(const std::string_view text) {
    const auto raw = bytes(text);
    std::vector<LzmwEncoderEntry> workspace(
        lzmw_encoder_workspace_entries(raw.size(), {}));
    const auto plan = plan_lzmw_token_stream(raw, {}, {}, workspace);
    EXPECT_EQ(plan.error, LzmwEncodeError::none);
    std::vector<std::byte> result(plan.output_size);
    EXPECT_EQ(encode_lzmw_token_stream(
                  raw, {}, {}, workspace, result).error,
              LzmwEncodeError::none);
    return result;
}

struct DecoderStorage {
    explicit DecoderStorage(const std::size_t raw_size) {
        requirements = lzmw_streaming_decoder_workspace_requirements(
            raw_size, {});
        encoded.resize(requirements.encoded_bytes);
        phrases.resize(requirements.phrase_entries);
        expansion.resize(requirements.expansion_entries);
        decoded.resize(requirements.decoded_bytes);
    }

    LzmwStreamingDecoderWorkspaceRequirements requirements{};
    std::vector<std::byte> encoded;
    std::vector<LzmwPhraseEntry> phrases;
    std::vector<std::uint32_t> expansion;
    std::vector<std::byte> decoded;
};

LzmwStreamingDecoder make_decoder(
    const std::size_t raw_size, DecoderStorage& storage,
    const marc::core::DecoderLimits limits = {}) {
    return {{}, raw_size, limits, storage.encoded, storage.phrases,
            storage.expansion, storage.decoded};
}

TEST(LzmwStreamingDecoder, ReportsConservativeWorkspaceRequirements) {
    const auto empty = lzmw_streaming_decoder_workspace_requirements(0, {});
    EXPECT_TRUE(empty.supported);
    EXPECT_EQ(empty.encoded_bytes, 0U);
    EXPECT_EQ(empty.phrase_entries, 0U);
    EXPECT_EQ(empty.expansion_entries, 0U);
    EXPECT_EQ(empty.decoded_bytes, 0U);

    const auto one = lzmw_streaming_decoder_workspace_requirements(1, {});
    EXPECT_TRUE(one.supported);
    EXPECT_EQ(one.encoded_bytes, 4U);
    EXPECT_EQ(one.phrase_entries, 0U);
    EXPECT_EQ(one.expansion_entries, 1U);
    EXPECT_EQ(one.decoded_bytes, 1U);

    LzmwParameters frozen{};
    frozen.maximum_entries = 1;
    const auto ten = lzmw_streaming_decoder_workspace_requirements(10, frozen);
    EXPECT_TRUE(ten.supported);
    EXPECT_EQ(ten.encoded_bytes, 40U);
    EXPECT_EQ(ten.phrase_entries, 1U);
    EXPECT_EQ(ten.expansion_entries, 2U);
    EXPECT_EQ(ten.decoded_bytes, 10U);

    EXPECT_FALSE(lzmw_streaming_decoder_workspace_requirements(
                     UINT64_MAX, {}).supported);
}

TEST(LzmwStreamingDecoder, DecodesOneByteInputAndOutput) {
    constexpr std::string_view raw = "abbaababaaba";
    const auto input = encoded(raw);
    DecoderStorage storage{raw.size()};
    auto decoder = make_decoder(raw.size(), storage);
    std::vector<std::byte> output_bytes;
    std::array<std::byte, 1> output{};
    std::size_t position{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, input.size() - position);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            position, count);
        const auto flags = position + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0)
            output_bytes.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, input.size());
    EXPECT_EQ(output_bytes, bytes(raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwStreamingDecoder, PreservesEndInputWhileDraining) {
    constexpr std::string_view raw = "ABABABABAB";
    const auto input = encoded(raw);
    DecoderStorage storage{raw.size()};
    auto decoder = make_decoder(raw.size(), storage);
    std::array<std::byte, raw.size()> output{};
    auto result = decoder.process(
        input, std::span<std::byte>{output}.first(1),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, input.size());
    std::size_t produced = result.output_produced;
    while (result.status != marc::core::StreamStatus::end_of_stream) {
        result = decoder.process(
            {}, std::span<std::byte>{output}.subspan(produced), 0);
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        produced += result.output_produced;
    }
    EXPECT_EQ(produced, output.size());
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes(raw));
}

TEST(LzmwStreamingDecoder, AcceptsEndInputWithZeroFinalBytesAndEmptyFrame) {
    const auto input = encoded("ABAB");
    DecoderStorage storage{4};
    auto decoder = make_decoder(4, storage);
    auto result = decoder.process(
        input, {}, marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(result.status, marc::core::StreamStatus::progress);
    EXPECT_EQ(result.input_consumed, input.size());
    std::array<std::byte, 4> output{};
    result = decoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(std::vector(output.begin(), output.end()), bytes("ABAB"));

    DecoderStorage empty_storage{0};
    auto empty = make_decoder(0, empty_storage);
    result = empty.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

TEST(LzmwStreamingDecoder, RejectsMalformedFrameWithoutPublishing) {
    auto input = encoded("ABAB");
    input.pop_back();
    DecoderStorage storage{4};
    auto decoder = make_decoder(4, storage);
    std::array<std::byte, 4> output{};
    output.fill(std::byte{0xcc});
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.error.byte_position, 2 * lzmw_token_size);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
    const auto repeated = decoder.process({}, output, 0);
    EXPECT_EQ(repeated.error.code, result.error.code);
    EXPECT_EQ(repeated.error.byte_position, result.error.byte_position);
    EXPECT_EQ(repeated.output_produced, 0U);
}

TEST(LzmwStreamingDecoder, RejectsExcessInputBeforeConsumption) {
    DecoderStorage storage{1};
    auto decoder = make_decoder(1, storage);
    std::array<std::byte, lzmw_token_size + 1> input{};
    std::array<std::byte, 1> output{};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
}

TEST(LzmwStreamingDecoder, RejectsTrailingInputWhileDraining) {
    const auto input = encoded("ABAB");
    DecoderStorage storage{4};
    auto decoder = make_decoder(4, storage);
    std::array<std::byte, 1> output{};
    auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::need_output);
    const std::array trailing{std::byte{0}};
    result = decoder.process(trailing, output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.input_consumed, 0U);
    EXPECT_EQ(result.output_produced, 0U);
}

TEST(LzmwStreamingDecoder, RejectsUnsupportedFlagsAndBadConstruction) {
    DecoderStorage storage{2};
    auto decoder = make_decoder(2, storage);
    auto result = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);

    LzmwStreamingDecoder short_encoded{
        {}, 2, {}, {}, storage.phrases, storage.expansion, storage.decoded};
    result = short_encoded.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
    LzmwStreamingDecoder short_phrases{
        {}, 2, {}, storage.encoded, {}, storage.expansion, storage.decoded};
    result = short_phrases.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
    LzmwStreamingDecoder short_expansion{
        {}, 2, {}, storage.encoded, storage.phrases, {}, storage.decoded};
    result = short_expansion.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);
    LzmwStreamingDecoder short_decoded{
        {}, 2, {}, storage.encoded, storage.phrases, storage.expansion, {}};
    result = short_decoded.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    marc::core::DecoderLimits limits{};
    limits.max_internal_buffered_bytes =
        storage.requirements.encoded_bytes
        + storage.requirements.phrase_entries * sizeof(LzmwPhraseEntry)
        + storage.requirements.expansion_entries * sizeof(std::uint32_t)
        + storage.requirements.decoded_bytes - 1;
    limits.max_block_size = limits.max_internal_buffered_bytes;
    auto limited = make_decoder(2, storage, limits);
    result = limited.process({}, {}, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

} // namespace

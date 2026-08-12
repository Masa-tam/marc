#include "frame/lzss_rans_frame_streaming_encoder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array input{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'},
    std::byte{'X'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 5;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference() {
    const auto stream = config(input.size());
    const marc::core::DecoderLimits limits{};
    std::vector<std::byte> output(lzss_rans_stream_prefix_size);
    EXPECT_EQ(serialize_stream_header(
                  stream, limits,
                  std::span<std::byte, stream_header_size>{
                      output.data(), stream_header_size}),
              StreamHeaderError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lzss_parameters(
                  {}, limits,
                  std::span<std::byte,
                            marc::dictionary::internal::lzss_parameter_size>{
                      output.data() + stream_header_size,
                      marc::dictionary::internal::lzss_parameter_size}),
              marc::dictionary::internal::LzssFormatError::none);

    std::array<std::byte, 4> staging{};
    std::uint64_t sequence{};
    std::uint64_t committed{};
    while (committed < input.size()) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(
                stream.frame_size, input.size() - committed));
        const auto raw = std::span<const std::byte>{input}.subspan(
            static_cast<std::size_t>(committed), count);
        const auto plan = plan_lzss_rans_frame(
            stream, {}, limits, sequence, committed, raw, staging);
        EXPECT_EQ(plan.error, LzssRansFrameValidationError::none);
        const auto offset = output.size();
        output.resize(offset + plan.serialized_size);
        EXPECT_EQ(encode_lzss_rans_frame(
                      stream, {}, limits, sequence, committed, raw, staging,
                      std::span<std::byte>{output}.subspan(
                          offset, plan.serialized_size)).error,
                  LzssRansFrameValidationError::none);
        committed += count;
        ++sequence;
    }
    return output;
}

} // namespace

TEST(LzssRansFrameStreamingEncoder, MatchesReferenceWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    LzssRansFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    std::vector<std::byte> actual;
    std::size_t input_offset{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(
            1, input.size() - input_offset);
        const auto chunk = std::span<const std::byte>{input}.subspan(
            input_offset, count);
        const auto flags = input_offset + count == input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(chunk, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, chunk.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        input_offset += result.input_consumed;
        if (result.output_produced != 0) {
            actual.push_back(output[0]);
        }
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(input_offset, input.size());
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(LzssRansFrameStreamingEncoder, EmitsFullFramesAndKeepsFlushOpen) {
    const auto expected = reference();
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    LzssRansFrameStreamingEncoder encoder{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{input}.first<1>(), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 1U);
    EXPECT_EQ(first.output_produced, lzss_rans_stream_prefix_size);
    EXPECT_EQ(first.status, marc::core::StreamStatus::progress);
    const auto second = encoder.process(
        std::span<const std::byte>{input}.subspan(1),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(LzssRansFrameStreamingEncoder, ReportsWorkspaceAndAggregateErrors) {
    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    std::array<std::byte, 16384> output{};
    LzssRansFrameStreamingEncoder short_input{
        config(input.size()), {}, {},
        std::span<std::byte>{frame_input}.first<1>(), dictionary_staging,
        frame_encoded};
    EXPECT_EQ(short_input.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);

    LzssRansFrameStreamingEncoder short_dictionary{
        config(input.size()), {}, {}, frame_input,
        std::span<std::byte>{dictionary_staging}.first<3>(), frame_encoded};
    EXPECT_EQ(short_dictionary.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);

    LzssRansFrameStreamingEncoder short_encoded{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        std::span<std::byte>{frame_encoded}.first<1>()};
    auto result = short_encoded.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 4> plan_staging{};
    const auto plan = plan_lzss_rans_frame(
        config(input.size()), {}, {}, 0, 0,
        std::span<const std::byte>{input}.first<2>(), plan_staging);
    ASSERT_EQ(plan.error, LzssRansFrameValidationError::none);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 5;
    limits.max_internal_buffered_bytes =
        2 + plan.dictionary_size + plan.serialized_size - 1;
    LzssRansFrameStreamingEncoder aggregate_limited{
        config(input.size()), {}, limits, frame_input, dictionary_staging,
        frame_encoded};
    result = aggregate_limited.process(
        std::span<const std::byte>{input}.first<2>(), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::limit_exceeded);
}

TEST(LzssRansFrameStreamingEncoder, HandlesEmptyAndProtocolErrors) {
    std::array<std::byte, 1> unused{};
    std::array<std::byte, lzss_rans_stream_prefix_size> output{};
    LzssRansFrameStreamingEncoder empty{
        config(0), {}, {}, std::span<std::byte>{}, std::span<std::byte>{},
        unused};
    auto result = empty.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, lzss_rans_stream_prefix_size);

    std::array<std::byte, 2> frame_input{};
    std::array<std::byte, 4> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    LzssRansFrameStreamingEncoder premature{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    result = premature.process(
        std::span<const std::byte>{input}.first<1>(), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    LzssRansFrameStreamingEncoder reset{
        config(input.size()), {}, {}, frame_input, dictionary_staging,
        frame_encoded};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
}

TEST(LzssRansFrameStreamingEncoder,
     HashChainMatchesOracleAndEnforcesFinderBoundaries) {
    constexpr std::array raw{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}, std::byte{'1'}, std::byte{'A'}, std::byte{'B'},
        std::byte{'C'}, std::byte{'D'}, std::byte{'E'}, std::byte{'2'}};
    auto stream = config(raw.size());
    stream.frame_size = static_cast<std::uint32_t>(raw.size());
    stream.entropy_block_size = 16;
    std::array<std::byte, raw.size()> frame_input{};
    std::array<std::byte, raw.size() * 2> dictionary_staging{};
    std::array<std::byte, 8192> frame_encoded{};
    const auto required = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(raw.size(), {}, {});
    ASSERT_EQ(required.error,
              marc::dictionary::internal::LzssHashChainError::none);
    std::vector<std::byte> allocation(
        required.workspace_size + required.workspace_alignment - 1);
    const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
    const auto remainder = address % required.workspace_alignment;
    const auto padding = remainder == 0
        ? std::size_t{0} : required.workspace_alignment - remainder;
    const auto finder = std::span<std::byte>{allocation}.subspan(
        padding, required.workspace_size);

    std::array<std::byte, raw.size() * 2> reference_staging{};
    const auto plan = plan_lzss_rans_frame(
        stream, {}, {}, 0, 0, raw, reference_staging);
    ASSERT_EQ(plan.error, LzssRansFrameValidationError::none);
    std::vector<std::byte> expected(
        lzss_rans_stream_prefix_size + plan.serialized_size);
    ASSERT_EQ(serialize_stream_header(
                  stream, {},
                  std::span<std::byte, stream_header_size>{
                      expected.data(), stream_header_size}),
              StreamHeaderError::none);
    ASSERT_EQ(marc::dictionary::internal::serialize_lzss_parameters(
                  {}, {},
                  std::span<std::byte,
                            marc::dictionary::internal::lzss_parameter_size>{
                      expected.data() + stream_header_size,
                      marc::dictionary::internal::lzss_parameter_size}),
              marc::dictionary::internal::LzssFormatError::none);
    ASSERT_EQ(encode_lzss_rans_frame(
                  stream, {}, {}, 0, 0, raw, reference_staging,
                  std::span<std::byte>{expected}.subspan(
                      lzss_rans_stream_prefix_size)).error,
              LzssRansFrameValidationError::none);

    LzssRansFrameStreamingEncoder encoder{
        stream, {}, {}, frame_input, dictionary_staging, finder,
        frame_encoded};
    std::vector<std::byte> actual(expected.size());
    const auto result = encoder.process(
        raw, actual,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    ASSERT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    actual.resize(result.output_produced);
    EXPECT_EQ(actual, expected);

    LzssRansFrameStreamingEncoder short_finder{
        stream, {}, {}, frame_input, dictionary_staging,
        finder.first(finder.size() - 1), frame_encoded};
    std::array<std::byte, 8192> output{};
    EXPECT_EQ(short_finder.process(raw, output, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    LzssRansFrameStreamingEncoder output_alias{
        stream, {}, {}, frame_input, dictionary_staging, finder,
        frame_encoded};
    EXPECT_EQ(output_alias.process({}, finder, 0).error.code,
              marc::core::ErrorCode::invalid_argument);
    LzssRansFrameStreamingEncoder constructor_alias{
        stream, {}, {}, frame_input, dictionary_staging,
        dictionary_staging, frame_encoded};
    EXPECT_EQ(constructor_alias.process({}, {}, 0).error.code,
              marc::core::ErrorCode::invalid_argument);
}

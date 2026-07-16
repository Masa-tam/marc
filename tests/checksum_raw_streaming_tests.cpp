#include "frame/checksum_raw_streaming_decoder.hpp"
#include "frame/checksum_raw_streaming_encoder.hpp"

#include "core/crc32c.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace {

using marc::frame::ChecksumRawStreamingDecoder;
using marc::frame::ChecksumRawStreamingEncoder;
using marc::frame::HashDescriptor;
using marc::frame::HashScope;
using marc::frame::HashTarget;
using marc::frame::StreamHeader;

constexpr HashDescriptor frame_crc32c{
    marc::core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 4, 0};
constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
    std::byte{'E'}, std::byte{'F'}, std::byte{'G'}};

[[nodiscard]] StreamHeader config(const std::uint64_t size) {
    StreamHeader stream{};
    stream.minor_version = marc::frame::hash_format_minor_version;
    stream.frame_size = 4;
    stream.hash_descriptors_size = marc::frame::hash_descriptor_size;
    stream.original_size = size;
    return stream;
}

[[nodiscard]] std::vector<std::byte> reference() {
    const auto stream = config(raw.size());
    const std::array descriptors{frame_crc32c};
    const auto plan = marc::frame::plan_checksum_raw_stream_v1_1(
        stream, descriptors, {}, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_checksum_raw_stream_v1_1(
                  stream, descriptors, {}, raw, encoded).error,
              marc::frame::ChecksumRawStreamError::none);
    return encoded;
}

TEST(ChecksumRawStreamingEncoder, MatchesOneShotWithOneByteBuffers) {
    const auto expected = reference();
    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingEncoder encoder{
        config(raw.size()), frame_crc32c, {}, workspace};
    std::vector<std::byte> encoded;
    std::size_t position{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, raw.size() - position);
        const auto input = std::span<const std::byte>{raw}.subspan(
            position, count);
        const auto flags = position + count == raw.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = encoder.process(input, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, input.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0) encoded.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, raw.size());
    EXPECT_EQ(encoded, expected);
    EXPECT_EQ(encoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(ChecksumRawStreamingEncoder, FlushDoesNotClosePartialFrame) {
    const auto expected = reference();
    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingEncoder encoder{
        config(raw.size()), frame_crc32c, {}, workspace};
    std::vector<std::byte> output(expected.size());
    const auto first = encoder.process(
        std::span<const std::byte>{raw}.first(2), output,
        marc::core::flag_value(marc::core::ProcessFlags::flush));
    EXPECT_EQ(first.input_consumed, 2U);
    EXPECT_EQ(first.output_produced,
              marc::frame::checksum_raw_stream_prefix_size);
    const auto second = encoder.process(
        std::span<const std::byte>{raw}.subspan(2),
        std::span<std::byte>{output}.subspan(first.output_produced),
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(second.status, marc::core::StreamStatus::end_of_stream);
    output.resize(first.output_produced + second.output_produced);
    EXPECT_EQ(output, expected);
}

TEST(ChecksumRawStreamingEncoder, RejectsWorkspacePrematureEndAndReset) {
    std::array<std::byte, 59> short_workspace{};
    std::array<std::byte, 256> output{};
    ChecksumRawStreamingEncoder short_encoder{
        config(raw.size()), frame_crc32c, {}, short_workspace};
    auto result = short_encoder.process(
        std::span<const std::byte>{raw}.first(4), output, 0);
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::out_of_memory);

    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingEncoder premature{
        config(raw.size()), frame_crc32c, {}, workspace};
    result = premature.process(
        std::span<const std::byte>{raw}.first(2), {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::invalid_argument);
    EXPECT_EQ(result.input_consumed, 0U);

    ChecksumRawStreamingEncoder reset{
        config(raw.size()), frame_crc32c, {}, workspace};
    result = reset.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::reset_block));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::unsupported);
    EXPECT_EQ(reset.process({}, {}, 0).error.code,
              marc::core::ErrorCode::unsupported);
}

TEST(ChecksumRawStreamingEncoder, EmitsOnlyPrefixForEmptyInput) {
    std::array<std::byte, 1> workspace{};
    std::array<std::byte, marc::frame::checksum_raw_stream_prefix_size>
        output{};
    ChecksumRawStreamingEncoder encoder{
        config(0), frame_crc32c, {}, workspace};
    const auto result = encoder.process(
        {}, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(result.output_produced, output.size());
    EXPECT_EQ(output[6], std::byte{1});
    EXPECT_EQ(output[64], std::byte{1});
}

TEST(ChecksumRawStreamingDecoder, DecodesOneByteInputAndOutput) {
    const auto encoded = reference();
    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingDecoder decoder{{}, workspace};
    std::vector<std::byte> decoded;
    std::size_t position{};
    std::array<std::byte, 1> output{};
    marc::core::StreamStatus status{};
    do {
        const auto count = std::min<std::size_t>(1, encoded.size() - position);
        const auto input = std::span<const std::byte>{encoded}.subspan(
            position, count);
        const auto flags = position + count == encoded.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
        const auto result = decoder.process(input, output, flags);
        ASSERT_TRUE(marc::core::is_valid(
            result, input.size(), output.size()));
        ASSERT_NE(result.status, marc::core::StreamStatus::error);
        position += result.input_consumed;
        if (result.output_produced != 0) decoded.push_back(output[0]);
        status = result.status;
    } while (status != marc::core::StreamStatus::end_of_stream);
    EXPECT_EQ(position, encoded.size());
    EXPECT_TRUE(std::ranges::equal(decoded, raw));
    EXPECT_EQ(decoder.process({}, {}, 0).status,
              marc::core::StreamStatus::end_of_stream);
}

TEST(ChecksumRawStreamingDecoder, LatchesEndInputWhileFrameDrains) {
    const auto encoded = reference();
    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingDecoder decoder{{}, workspace};
    constexpr std::size_t first_extent =
        marc::frame::checksum_raw_stream_prefix_size + 56 + 4 + 4;
    std::array<std::byte, 4> first_frame{};
    auto result = decoder.process(
        std::span<const std::byte>{encoded}.first(first_extent), first_frame,
        0);
    ASSERT_EQ(result.input_consumed, first_extent);
    ASSERT_EQ(result.output_produced, first_frame.size());
    ASSERT_EQ(result.status, marc::core::StreamStatus::progress);

    std::array<std::byte, 1> first{};
    result = decoder.process(
        std::span<const std::byte>{encoded}.subspan(first_extent), first,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::need_output);
    EXPECT_EQ(result.input_consumed, encoded.size() - first_extent);
    EXPECT_EQ(result.output_produced, 1U);

    std::array<std::byte, 2> rest{};
    result = decoder.process({}, rest, 0);
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    std::array<std::byte, raw.size()> decoded{};
    std::ranges::copy(first_frame, decoded.begin());
    decoded[4] = first[0];
    std::ranges::copy(rest, decoded.begin() + 5);
    EXPECT_EQ(decoded, raw);
}

TEST(ChecksumRawStreamingDecoder, SuppressesCorruptedFrameOnly) {
    auto encoded = reference();
    constexpr std::size_t first_frame_size = 56 + 4 + 4;
    const auto second_frame = marc::frame::checksum_raw_stream_prefix_size
        + first_frame_size;
    encoded[second_frame + 56] ^= std::byte{1};

    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingDecoder decoder{{}, workspace};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    const auto result = decoder.process(
        encoded, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.error.code, marc::core::ErrorCode::malformed_stream);
    EXPECT_EQ(result.output_produced, 4U);
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{output}.first(4),
        std::span<const std::byte>{raw}.first(4)));
    EXPECT_TRUE(std::ranges::all_of(
        std::span<const std::byte>{output}.subspan(4),
        [](const std::byte value) { return value == std::byte{0x5a}; }));
    EXPECT_EQ(decoder.process({}, {}, 0).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(ChecksumRawStreamingDecoder, RejectsTruncationWorkspaceAndTrailingData) {
    const auto canonical = reference();
    for (std::size_t size = 0; size < canonical.size(); ++size) {
        std::array<std::byte, 64> workspace{};
        ChecksumRawStreamingDecoder decoder{{}, workspace};
        std::array<std::byte, raw.size()> output{};
        const auto result = decoder.process(
            std::span<const std::byte>{canonical}.first(size), output,
            marc::core::flag_value(marc::core::ProcessFlags::end_input));
        EXPECT_EQ(result.status, marc::core::StreamStatus::error) << size;
    }

    std::array<std::byte, 59> short_workspace{};
    ChecksumRawStreamingDecoder short_decoder{{}, short_workspace};
    std::array<std::byte, raw.size()> output{};
    EXPECT_EQ(short_decoder.process(canonical, output, 0).error.code,
              marc::core::ErrorCode::out_of_memory);

    auto trailing = canonical;
    trailing.push_back(std::byte{});
    std::array<std::byte, 64> workspace{};
    ChecksumRawStreamingDecoder trailing_decoder{{}, workspace};
    EXPECT_EQ(trailing_decoder.process(
                  trailing, output,
                  marc::core::flag_value(
                      marc::core::ProcessFlags::end_input)).error.code,
              marc::core::ErrorCode::malformed_stream);
}

TEST(ChecksumRawStreamingDecoder, HandlesEmptyStream) {
    const std::array<std::byte, 0> empty{};
    const auto stream = config(0);
    const std::array descriptors{frame_crc32c};
    const auto plan = marc::frame::plan_checksum_raw_stream_v1_1(
        stream, descriptors, {}, empty);
    std::vector<std::byte> encoded(plan.serialized_size);
    ASSERT_EQ(marc::frame::encode_checksum_raw_stream_v1_1(
                  stream, descriptors, {}, empty, encoded).error,
              marc::frame::ChecksumRawStreamError::none);
    std::array<std::byte, 1> workspace{};
    ChecksumRawStreamingDecoder decoder{{}, workspace};
    const auto result = decoder.process(
        encoded, {},
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
}

} // namespace

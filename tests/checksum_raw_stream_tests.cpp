#include "frame/checksum_raw_stream.hpp"

#include "core/crc32c.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using marc::frame::ChecksumRawStreamError;
using marc::frame::HashDescriptor;
using marc::frame::HashScope;
using marc::frame::HashTarget;
using marc::frame::StreamHeader;

constexpr HashDescriptor frame_crc32c{
    marc::core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 4, 0};

[[nodiscard]] StreamHeader config(const std::uint64_t size,
                                  const std::uint32_t frame_size = 3) {
    StreamHeader stream{};
    stream.minor_version = marc::frame::hash_format_minor_version;
    stream.frame_size = frame_size;
    stream.hash_descriptors_size = marc::frame::hash_descriptor_size;
    stream.original_size = size;
    return stream;
}

template<std::size_t Size>
[[nodiscard]] std::vector<std::byte> encode(
    const std::array<std::byte, Size>& input,
    const std::uint32_t frame_size = 3) {
    const auto stream = config(input.size(), frame_size);
    const std::array descriptors{frame_crc32c};
    const auto plan = marc::frame::plan_checksum_raw_stream_v1_1(
        stream, descriptors, {}, input);
    EXPECT_EQ(plan.error, ChecksumRawStreamError::none);
    std::vector<std::byte> output(plan.serialized_size);
    EXPECT_EQ(marc::frame::encode_checksum_raw_stream_v1_1(
                  stream, descriptors, {}, input, output).error,
              ChecksumRawStreamError::none);
    return output;
}

constexpr std::array abc{
    std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
constexpr std::array abcde{
    std::byte{0x61}, std::byte{0x62}, std::byte{0x63},
    std::byte{0x64}, std::byte{0x65}};

TEST(ChecksumRawStream, EmitsHandCheckableSingleFrameLayout) {
    const auto stream = config(abc.size());
    const std::array descriptors{frame_crc32c};
    const auto plan = marc::frame::plan_checksum_raw_stream_v1_1(
        stream, descriptors, {}, abc);
    ASSERT_EQ(plan.error, ChecksumRawStreamError::none);
    EXPECT_EQ(plan.serialized_size, 143U);
    EXPECT_EQ(plan.frame_count, 1U);

    const auto encoded = encode(abc);
    EXPECT_EQ(encoded[6], std::byte{0x01});
    EXPECT_EQ(encoded[36], std::byte{0x10});
    EXPECT_EQ(encoded[64], std::byte{0x01});
    EXPECT_EQ(encoded[68], std::byte{0x01});
    EXPECT_EQ(encoded[69], std::byte{0x02});
    EXPECT_EQ(encoded[80 + 36], std::byte{0x04});
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(136, 3), abc));
    const std::array expected_crc{
        std::byte{0xb7}, std::byte{0x3f},
        std::byte{0x4b}, std::byte{0x36}};
    EXPECT_TRUE(std::ranges::equal(
        std::span<const std::byte>{encoded}.subspan(139, 4), expected_crc));
}

TEST(ChecksumRawStream, RoundTripsThreeIndependentFrames) {
    const auto encoded = encode(abcde, 2);
    EXPECT_EQ(encoded.size(), 265U);
    std::array<std::byte, abcde.size()> output{};
    StreamHeader stream{};
    HashDescriptor descriptor{};
    const auto result = marc::frame::decode_checksum_raw_stream_v1_1(
        encoded, {}, output, stream, descriptor);
    EXPECT_EQ(result.error, ChecksumRawStreamError::none);
    EXPECT_EQ(result.frame_count, 3U);
    EXPECT_EQ(output, abcde);
    EXPECT_EQ(stream.frame_size, 2U);
    EXPECT_EQ(descriptor.algorithm_id, marc::core::crc32c_algorithm_id);
}

TEST(ChecksumRawStream, EmptyInputIsExactlyPrefixAndDescriptor) {
    const std::array<std::byte, 0> input{};
    const auto encoded = encode(input);
    EXPECT_EQ(encoded.size(), marc::frame::checksum_raw_stream_prefix_size);
    StreamHeader stream{};
    HashDescriptor descriptor{};
    const auto result = marc::frame::decode_checksum_raw_stream_v1_1(
        encoded, {}, {}, stream, descriptor);
    EXPECT_EQ(result.error, ChecksumRawStreamError::none);
    EXPECT_EQ(result.frame_count, 0U);
    EXPECT_EQ(stream.original_size, 0U);
}

TEST(ChecksumRawStream, EncodingIsDeterministicAndShortOutputIsAtomic) {
    const auto first = encode(abcde, 2);
    const auto second = encode(abcde, 2);
    EXPECT_EQ(first, second);

    const auto stream = config(abcde.size(), 2);
    const std::array descriptors{frame_crc32c};
    std::vector<std::byte> short_output(first.size() - 1, std::byte{0xa5});
    EXPECT_EQ(marc::frame::encode_checksum_raw_stream_v1_1(
                  stream, descriptors, {}, abcde, short_output).error,
              ChecksumRawStreamError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(short_output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));

    auto wrong_size = stream;
    ++wrong_size.original_size;
    EXPECT_EQ(marc::frame::plan_checksum_raw_stream_v1_1(
                  wrong_size, descriptors, {}, abcde).error,
              ChecksumRawStreamError::input_size_mismatch);
}

TEST(ChecksumRawStream, RejectsEveryStrictTruncationTransactionally) {
    const auto encoded = encode(abc);
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        std::array<std::byte, abc.size()> output{};
        output.fill(std::byte{0xa5});
        StreamHeader stream{};
        stream.original_size = 999;
        HashDescriptor descriptor{};
        descriptor.algorithm_id = 999;
        const auto result = marc::frame::decode_checksum_raw_stream_v1_1(
            std::span<const std::byte>{encoded}.first(size), {}, output,
            stream, descriptor);
        EXPECT_NE(result.error, ChecksumRawStreamError::none) << size;
        EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
            return value == std::byte{0xa5};
        })) << size;
        EXPECT_EQ(stream.original_size, 999U) << size;
        EXPECT_EQ(descriptor.algorithm_id, 999U) << size;
    }
}

TEST(ChecksumRawStream, RejectsMetadataPayloadAndChecksumCorruption) {
    const auto canonical = encode(abc);
    const auto check_atomic_failure = [](
        std::vector<std::byte> corrupted,
        const ChecksumRawStreamError expected) {
        std::array<std::byte, abc.size()> output{};
        output.fill(std::byte{0xa5});
        StreamHeader stream{};
        stream.original_size = 999;
        HashDescriptor descriptor{};
        descriptor.algorithm_id = 999;
        const auto result = marc::frame::decode_checksum_raw_stream_v1_1(
            corrupted, {}, output, stream, descriptor);
        EXPECT_EQ(result.error, expected);
        EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
            return value == std::byte{0xa5};
        }));
        EXPECT_EQ(stream.original_size, 999U);
        EXPECT_EQ(descriptor.algorithm_id, 999U);
    };

    auto corrupted = canonical;
    corrupted[64 + 8] = std::byte{0x01};
    check_atomic_failure(corrupted,
                         ChecksumRawStreamError::invalid_descriptor_region);
    corrupted = canonical;
    corrupted[80 + 24] = std::byte{0x02};
    check_atomic_failure(corrupted,
                         ChecksumRawStreamError::frame_header_error);
    corrupted = canonical;
    corrupted[136] ^= std::byte{0x01};
    check_atomic_failure(corrupted, ChecksumRawStreamError::checksum_error);
    for (std::size_t index = 0; index < 4; ++index) {
        corrupted = canonical;
        corrupted[139 + index] ^= std::byte{0x01};
        check_atomic_failure(corrupted,
                             ChecksumRawStreamError::checksum_error);
    }
}

TEST(ChecksumRawStream, LaterFrameCorruptionPublishesNoPrefix) {
    auto encoded = encode(abcde, 2);
    encoded.back() ^= std::byte{0x01};
    std::array<std::byte, abcde.size()> output{};
    output.fill(std::byte{0xa5});
    StreamHeader stream{};
    HashDescriptor descriptor{};
    const auto result = marc::frame::decode_checksum_raw_stream_v1_1(
        encoded, {}, output, stream, descriptor);
    EXPECT_EQ(result.error, ChecksumRawStreamError::checksum_error);
    EXPECT_EQ(result.frame_index, 2U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(ChecksumRawStream, RejectsTrailingShortOutputVersionAndPipeline) {
    const auto canonical = encode(abc);
    auto trailing = canonical;
    trailing.push_back(std::byte{});
    std::array<std::byte, abc.size()> output{};
    StreamHeader stream{};
    HashDescriptor descriptor{};
    EXPECT_EQ(marc::frame::decode_checksum_raw_stream_v1_1(
                  trailing, {}, output, stream, descriptor).error,
              ChecksumRawStreamError::trailing_stream_bytes);

    EXPECT_EQ(marc::frame::decode_checksum_raw_stream_v1_1(
                  canonical, {}, std::span<std::byte>{output}.first(2),
                  stream, descriptor).error,
              ChecksumRawStreamError::output_too_small);

    auto version_1_0 = canonical;
    version_1_0[6] = std::byte{};
    EXPECT_EQ(marc::frame::decode_checksum_raw_stream_v1_1(
                  version_1_0, {}, output, stream, descriptor).error,
              ChecksumRawStreamError::invalid_stream_header);

    auto unsupported = canonical;
    unsupported[12] = std::byte{0x01};
    unsupported[14] = std::byte{0x01};
    EXPECT_EQ(marc::frame::decode_checksum_raw_stream_v1_1(
                  unsupported, {}, output, stream, descriptor).error,
              ChecksumRawStreamError::unsupported_pipeline);
}

} // namespace

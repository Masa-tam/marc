#include "frame/hash_descriptor.hpp"

#include "core/crc32c.hpp"
#include "core/sha256.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::frame::HashDescriptor;
using marc::frame::HashDescriptorError;
using marc::frame::HashDescriptorRegionError;
using marc::frame::HashScope;
using marc::frame::HashTarget;

constexpr HashDescriptor sha256_whole_uncompressed{
    marc::core::sha256_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::whole_stream, 32, 0};
constexpr HashDescriptor crc32c_frame_uncompressed{
    marc::core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 4, 0};
constexpr HashDescriptor sha256_frame_uncompressed{
    marc::core::sha256_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 32, 0};

TEST(HashDescriptor, SerializesHandCheckableCrc32cPerFrameVector) {
    const HashDescriptor descriptor{
        marc::core::crc32c_algorithm_id,
        HashTarget::uncompressed_bytes,
        HashScope::per_frame,
        static_cast<std::uint16_t>(marc::core::crc32c_digest_size),
        0};
    std::array<std::byte, marc::frame::hash_descriptor_size> output{};
    ASSERT_EQ(marc::frame::serialize_hash_descriptor(descriptor, output),
              HashDescriptorError::none);
    const std::array expected{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x02}, std::byte{0x04}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(output, expected);
}

TEST(HashDescriptor, ParsesHandCheckableSha256WholeStreamVector) {
    const std::array input{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x01}, std::byte{0x20}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    HashDescriptor descriptor{};
    ASSERT_EQ(marc::frame::parse_hash_descriptor(input, descriptor),
              HashDescriptorError::none);
    EXPECT_EQ(descriptor.algorithm_id, marc::core::sha256_algorithm_id);
    EXPECT_EQ(descriptor.target, HashTarget::uncompressed_bytes);
    EXPECT_EQ(descriptor.scope, HashScope::whole_stream);
    EXPECT_EQ(descriptor.digest_size, marc::core::sha256_digest_size);
    EXPECT_EQ(descriptor.flags, 0U);
}

TEST(HashDescriptor, RejectsUnknownAlgorithmWithoutPublishingOutput) {
    std::array<std::byte, marc::frame::hash_descriptor_size> input{};
    input[0] = std::byte{0xff};
    input[4] = std::byte{0x01};
    input[5] = std::byte{0x01};
    input[6] = std::byte{0x04};
    HashDescriptor descriptor{
        marc::core::sha256_algorithm_id, HashTarget::compressed_payload,
        HashScope::per_block, 32, 0};
    EXPECT_EQ(marc::frame::parse_hash_descriptor(input, descriptor),
              HashDescriptorError::unknown_algorithm);
    EXPECT_EQ(descriptor.algorithm_id, marc::core::sha256_algorithm_id);
    EXPECT_EQ(descriptor.target, HashTarget::compressed_payload);
    EXPECT_EQ(descriptor.scope, HashScope::per_block);
}

TEST(HashDescriptor, RejectsInvalidTargetScopeDigestAndFlags) {
    HashDescriptor descriptor{
        marc::core::crc32c_algorithm_id, HashTarget{}, HashScope::per_frame,
        4, 0};
    EXPECT_EQ(marc::frame::validate_hash_descriptor(descriptor),
              HashDescriptorError::invalid_target);
    descriptor.target = HashTarget::uncompressed_bytes;
    descriptor.scope = HashScope{};
    EXPECT_EQ(marc::frame::validate_hash_descriptor(descriptor),
              HashDescriptorError::invalid_scope);
    descriptor.scope = HashScope::per_frame;
    descriptor.digest_size = 32;
    EXPECT_EQ(marc::frame::validate_hash_descriptor(descriptor),
              HashDescriptorError::invalid_digest_size);
    descriptor.digest_size = 4;
    descriptor.flags = 1;
    EXPECT_EQ(marc::frame::validate_hash_descriptor(descriptor),
              HashDescriptorError::unknown_flags);
}

TEST(HashDescriptor, RejectsEveryReservedByteWithoutPublishingOutput) {
    const std::array canonical{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x02}, std::byte{0x04}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    for (std::size_t offset = 12; offset < canonical.size(); ++offset) {
        auto input = canonical;
        input[offset] = std::byte{0x01};
        HashDescriptor descriptor{
            marc::core::sha256_algorithm_id, HashTarget::compressed_payload,
            HashScope::per_block, 32, 0};
        EXPECT_EQ(marc::frame::parse_hash_descriptor(input, descriptor),
                  HashDescriptorError::nonzero_reserved) << offset;
        EXPECT_EQ(descriptor.algorithm_id, marc::core::sha256_algorithm_id)
            << offset;
    }
}

TEST(HashDescriptor, InvalidSerializationLeavesDestinationUnchanged) {
    const HashDescriptor descriptor{
        marc::core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
        HashScope::per_frame, 32, 0};
    std::array<std::byte, marc::frame::hash_descriptor_size> output{};
    output.fill(std::byte{0xa5});
    const auto unchanged = output;
    EXPECT_EQ(marc::frame::serialize_hash_descriptor(descriptor, output),
              HashDescriptorError::invalid_digest_size);
    EXPECT_EQ(output, unchanged);
}

TEST(HashDescriptorRegion, SerializesAndParsesCanonicalOrder) {
    const std::array descriptors{
        sha256_whole_uncompressed,
        crc32c_frame_uncompressed,
        sha256_frame_uncompressed};
    std::array<std::byte, 48> bytes{};
    ASSERT_EQ(marc::frame::serialize_hash_descriptor_region(descriptors, bytes),
              HashDescriptorRegionError::none);

    EXPECT_EQ(bytes[0], std::byte{0x02});
    EXPECT_EQ(bytes[4], std::byte{0x01});
    EXPECT_EQ(bytes[5], std::byte{0x01});
    EXPECT_EQ(bytes[16], std::byte{0x01});
    EXPECT_EQ(bytes[21], std::byte{0x02});
    EXPECT_EQ(bytes[32], std::byte{0x02});

    std::array<HashDescriptor, 3> parsed{};
    std::size_t count = 99;
    ASSERT_EQ(marc::frame::parse_hash_descriptor_region(bytes, parsed, count),
              HashDescriptorRegionError::none);
    EXPECT_EQ(count, descriptors.size());
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        EXPECT_EQ(parsed[index].algorithm_id, descriptors[index].algorithm_id);
        EXPECT_EQ(parsed[index].target, descriptors[index].target);
        EXPECT_EQ(parsed[index].scope, descriptors[index].scope);
        EXPECT_EQ(parsed[index].digest_size, descriptors[index].digest_size);
    }
}

TEST(HashDescriptorRegion, AcceptsEmptyRegionWithoutOutput) {
    std::array<HashDescriptor, 1> output{sha256_whole_uncompressed};
    std::size_t count = 7;
    ASSERT_EQ(marc::frame::parse_hash_descriptor_region({}, output, count),
              HashDescriptorRegionError::none);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(output[0].algorithm_id, marc::core::sha256_algorithm_id);
    EXPECT_EQ(marc::frame::serialize_hash_descriptor_region({}, {}),
              HashDescriptorRegionError::none);
}

TEST(HashDescriptorRegion, RejectsNonMultipleAndShortOutputTransactionally) {
    std::array<std::byte, 17> malformed{};
    std::array<HashDescriptor, 1> output{sha256_whole_uncompressed};
    std::size_t count = 7;
    EXPECT_EQ(marc::frame::parse_hash_descriptor_region(
                  malformed, output, count),
              HashDescriptorRegionError::invalid_region_size);
    EXPECT_EQ(count, 7U);

    std::array<std::byte, 32> two_records{};
    EXPECT_EQ(marc::frame::parse_hash_descriptor_region(
                  two_records, output, count),
              HashDescriptorRegionError::output_too_small);
    EXPECT_EQ(count, 7U);
    EXPECT_EQ(output[0].algorithm_id, marc::core::sha256_algorithm_id);
}

TEST(HashDescriptorRegion, RejectsMalformedRecordWithoutPublishingAnyRecord) {
    const std::array descriptors{
        sha256_whole_uncompressed, crc32c_frame_uncompressed};
    std::array<std::byte, 32> bytes{};
    ASSERT_EQ(marc::frame::serialize_hash_descriptor_region(descriptors, bytes),
              HashDescriptorRegionError::none);
    bytes[16 + 12] = std::byte{0x01};

    std::array<HashDescriptor, 2> output{
        sha256_frame_uncompressed, sha256_frame_uncompressed};
    std::size_t count = 9;
    EXPECT_EQ(marc::frame::parse_hash_descriptor_region(bytes, output, count),
              HashDescriptorRegionError::invalid_descriptor);
    EXPECT_EQ(count, 9U);
    EXPECT_EQ(output[0].scope, HashScope::per_frame);
    EXPECT_EQ(output[1].scope, HashScope::per_frame);
}

TEST(HashDescriptorRegion, RejectsDuplicateAndNoncanonicalKeys) {
    const std::array duplicate{
        sha256_whole_uncompressed, sha256_whole_uncompressed};
    EXPECT_EQ(marc::frame::validate_hash_descriptor_region(duplicate),
              HashDescriptorRegionError::duplicate_descriptor);

    const std::array reversed{
        crc32c_frame_uncompressed, sha256_whole_uncompressed};
    EXPECT_EQ(marc::frame::validate_hash_descriptor_region(reversed),
              HashDescriptorRegionError::noncanonical_order);
}

TEST(HashDescriptorRegion, FailedSerializationLeavesOutputUnchanged) {
    const std::array reversed{
        crc32c_frame_uncompressed, sha256_whole_uncompressed};
    std::array<std::byte, 32> output{};
    output.fill(std::byte{0xa5});
    const auto unchanged = output;
    EXPECT_EQ(marc::frame::serialize_hash_descriptor_region(reversed, output),
              HashDescriptorRegionError::noncanonical_order);
    EXPECT_EQ(output, unchanged);

    const std::array valid{sha256_whole_uncompressed};
    EXPECT_EQ(marc::frame::serialize_hash_descriptor_region(
                  valid, std::span<std::byte>{output}.first(15)),
              HashDescriptorRegionError::output_too_small);
    EXPECT_EQ(output, unchanged);
}

} // namespace

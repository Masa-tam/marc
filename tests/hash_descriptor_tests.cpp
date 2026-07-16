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
using marc::frame::HashScope;
using marc::frame::HashTarget;

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

} // namespace

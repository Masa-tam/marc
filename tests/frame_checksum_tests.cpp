#include "frame/frame_checksum.hpp"

#include "core/crc32c.hpp"
#include "core/sha256.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

using marc::frame::FrameChecksumError;
using marc::frame::HashDescriptor;
using marc::frame::HashScope;
using marc::frame::HashTarget;

constexpr HashDescriptor frame_crc32c{
    marc::core::crc32c_algorithm_id, HashTarget::uncompressed_bytes,
    HashScope::per_frame, 4, 0};

[[nodiscard]] std::span<const std::byte> bytes(const char* text,
                                               const std::size_t size) {
    return {reinterpret_cast<const std::byte*>(text), size};
}

TEST(FrameChecksum, AcceptsOnlyInitialV11DescriptorAndTrailerSize) {
    const std::array descriptors{frame_crc32c};
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  descriptors, 4),
              FrameChecksumError::none);
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1({}, 4),
              FrameChecksumError::invalid_descriptor_set);

    const std::array extra{frame_crc32c, frame_crc32c};
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(extra, 4),
              FrameChecksumError::invalid_descriptor_set);
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  descriptors, 0),
              FrameChecksumError::invalid_trailer_size);
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  descriptors, 5),
              FrameChecksumError::invalid_trailer_size);
}

TEST(FrameChecksum, RejectsEveryUnsupportedDescriptorProperty) {
    HashDescriptor descriptor = frame_crc32c;
    descriptor.algorithm_id = marc::core::sha256_algorithm_id;
    descriptor.digest_size = 32;
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  std::span{&descriptor, 1}, 4),
              FrameChecksumError::invalid_descriptor_set);

    descriptor = frame_crc32c;
    descriptor.target = HashTarget::compressed_payload;
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  std::span{&descriptor, 1}, 4),
              FrameChecksumError::invalid_descriptor_set);
    descriptor = frame_crc32c;
    descriptor.scope = HashScope::whole_stream;
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  std::span{&descriptor, 1}, 4),
              FrameChecksumError::invalid_descriptor_set);
    descriptor = frame_crc32c;
    descriptor.digest_size = 32;
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  std::span{&descriptor, 1}, 4),
              FrameChecksumError::invalid_descriptor_set);
    descriptor = frame_crc32c;
    descriptor.flags = 1;
    EXPECT_EQ(marc::frame::validate_frame_checksum_profile_v1_1(
                  std::span{&descriptor, 1}, 4),
              FrameChecksumError::invalid_descriptor_set);
}

TEST(FrameChecksum, GeneratesHandCheckableCrc32cTrailer) {
    const std::array descriptors{frame_crc32c};
    std::array<std::byte, 4> trailer{};
    ASSERT_EQ(marc::frame::generate_frame_checksum_v1_1(
                  bytes("123456789", 9), descriptors, 4, trailer),
              FrameChecksumError::none);
    const std::array expected{
        std::byte{0x83}, std::byte{0x92},
        std::byte{0x06}, std::byte{0xe3}};
    EXPECT_EQ(trailer, expected);
}

TEST(FrameChecksum, GeneratesAndVerifiesEmptyByteSpan) {
    const std::array descriptors{frame_crc32c};
    std::array<std::byte, 4> trailer{};
    ASSERT_EQ(marc::frame::generate_frame_checksum_v1_1(
                  {}, descriptors, 4, trailer),
              FrameChecksumError::none);
    EXPECT_EQ(trailer, (std::array<std::byte, 4>{}));
    EXPECT_EQ(marc::frame::verify_frame_checksum_v1_1(
                  {}, descriptors, 4, trailer),
              FrameChecksumError::none);
}

TEST(FrameChecksum, RejectsEveryTrailerByteCorruption) {
    const std::array descriptors{frame_crc32c};
    std::array<std::byte, 4> trailer{};
    ASSERT_EQ(marc::frame::generate_frame_checksum_v1_1(
                  bytes("123456789", 9), descriptors, 4, trailer),
              FrameChecksumError::none);
    for (std::size_t index = 0; index < trailer.size(); ++index) {
        auto corrupted = trailer;
        corrupted[index] ^= std::byte{0x01};
        EXPECT_EQ(marc::frame::verify_frame_checksum_v1_1(
                      bytes("123456789", 9), descriptors, 4, corrupted),
                  FrameChecksumError::checksum_mismatch) << index;
    }
}

TEST(FrameChecksum, FailuresLeaveTrailerOutputUnchanged) {
    const std::array descriptors{frame_crc32c};
    std::array<std::byte, 5> output{};
    output.fill(std::byte{0xa5});
    const auto unchanged = output;
    EXPECT_EQ(marc::frame::generate_frame_checksum_v1_1(
                  bytes("abc", 3), descriptors, 4, output),
              FrameChecksumError::invalid_trailer_size);
    EXPECT_EQ(output, unchanged);

    auto invalid = frame_crc32c;
    invalid.scope = HashScope::whole_stream;
    EXPECT_EQ(marc::frame::generate_frame_checksum_v1_1(
                  bytes("abc", 3), std::span{&invalid, 1}, 4,
                  std::span<std::byte>{output}.first(4)),
              FrameChecksumError::invalid_descriptor_set);
    EXPECT_EQ(output, unchanged);
}

TEST(FrameChecksum, VerificationRequiresExactTrailerExtent) {
    const std::array descriptors{frame_crc32c};
    std::array<std::byte, 5> trailer{};
    EXPECT_EQ(marc::frame::verify_frame_checksum_v1_1(
                  {}, descriptors, 4, trailer),
              FrameChecksumError::invalid_trailer_size);
}

} // namespace

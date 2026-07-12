#include "core/header_accumulator.hpp"
#include "frame/stream_header.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

[[nodiscard]] std::array<std::byte, marc::frame::stream_header_size>
empty_header_vector() {
    std::array<std::byte, marc::frame::stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x01};
    bytes[8] = std::byte{0x40};
    bytes[22] = std::byte{0x10};
    return bytes;
}

} // namespace

using marc::frame::StreamHeaderError;

TEST(StreamHeaderTest, SerializesHandCheckableEmptyVector) {
    const marc::frame::StreamHeader header{};
    std::array<std::byte, marc::frame::stream_header_size> output{};
    ASSERT_EQ(marc::frame::serialize_stream_header(
                  header, marc::core::DecoderLimits{}, output),
              StreamHeaderError::none);
    EXPECT_EQ(output, empty_header_vector());
}

TEST(StreamHeaderTest, ParsesHandCheckableEmptyVector) {
    marc::frame::StreamHeader header;
    ASSERT_EQ(marc::frame::parse_stream_header(
                  empty_header_vector(), marc::core::DecoderLimits{}, header),
              StreamHeaderError::none);
    EXPECT_EQ(header.dictionary_algorithm,
              marc::frame::DictionaryAlgorithm::none);
    EXPECT_EQ(header.entropy_algorithm, marc::frame::EntropyAlgorithm::none);
    EXPECT_EQ(header.frame_size, UINT32_C(1) << 20);
    EXPECT_EQ(header.original_size, 0U);
}

TEST(StreamHeaderTest, ParsesAfterEveryInputSplit) {
    const auto input = empty_header_vector();
    for (std::size_t split = 0; split <= input.size(); ++split) {
        marc::core::HeaderAccumulator<marc::frame::stream_header_size>
            accumulator;
        (void)accumulator.append(
            std::span<const std::byte>{input}.first(split));
        (void)accumulator.append(
            std::span<const std::byte>{input}.subspan(split));
        ASSERT_TRUE(accumulator.bytes().has_value()) << "split=" << split;

        marc::frame::StreamHeader header;
        EXPECT_EQ(marc::frame::parse_stream_header(
                      *accumulator.bytes(), marc::core::DecoderLimits{}, header),
                  StreamHeaderError::none)
            << "split=" << split;
    }
}

TEST(StreamHeaderTest, RejectsInvalidMagicWithoutChangingOutput) {
    auto input = empty_header_vector();
    input[0] = std::byte{0};
    marc::frame::StreamHeader output{};
    output.original_size = 123;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::invalid_magic);
    EXPECT_EQ(output.original_size, 123U);
}

TEST(StreamHeaderTest, RejectsUnknownVersion) {
    auto input = empty_header_vector();
    input[4] = std::byte{0x02};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::unsupported_version);
}

TEST(StreamHeaderTest, RejectsIncorrectFixedSize) {
    auto input = empty_header_vector();
    input[8] = std::byte{0x3f};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::invalid_header_size);
}

TEST(StreamHeaderTest, RejectsUnknownFlags) {
    auto input = empty_header_vector();
    input[10] = std::byte{0x01};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::unknown_flags);
}

TEST(StreamHeaderTest, RejectsUnknownAlgorithmIds) {
    auto input = empty_header_vector();
    input[12] = std::byte{0xff};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::unknown_dictionary_algorithm);

    input = empty_header_vector();
    input[16] = std::byte{0xff};
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::unknown_entropy_algorithm);
}

TEST(StreamHeaderTest, RejectsUnsupportedVariant) {
    auto input = empty_header_vector();
    input[14] = std::byte{0x01};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::unsupported_dictionary_variant);
}

TEST(StreamHeaderTest, RejectsNonzeroReservedByte) {
    auto input = empty_header_vector();
    input[63] = std::byte{0x01};
    marc::frame::StreamHeader output;
    EXPECT_EQ(marc::frame::parse_stream_header(
                  input, marc::core::DecoderLimits{}, output),
              StreamHeaderError::nonzero_reserved);
}

TEST(StreamHeaderTest, RejectsSizesBeyondLocalPolicy) {
    marc::frame::StreamHeader header{};
    const marc::core::DecoderLimits limits{};
    header.frame_size = static_cast<std::uint32_t>(limits.max_frame_size + 1);
    EXPECT_EQ(marc::frame::validate_stream_header(header, limits),
              StreamHeaderError::limit_exceeded);

    header = {};
    header.original_size = limits.max_total_output_size + 1;
    EXPECT_EQ(marc::frame::validate_stream_header(header, limits),
              StreamHeaderError::limit_exceeded);
}

TEST(StreamHeaderTest, ValidatesBufferedEntropyBlockRule) {
    marc::frame::StreamHeader header{};
    header.entropy_algorithm = marc::frame::EntropyAlgorithm::blocked_huffman;
    header.entropy_variant = 1;
    EXPECT_EQ(marc::frame::validate_stream_header(
                  header, marc::core::DecoderLimits{}),
              StreamHeaderError::contradictory_parameters);

    header.entropy_block_size = UINT32_C(1) << 16;
    EXPECT_EQ(marc::frame::validate_stream_header(
                  header, marc::core::DecoderLimits{}),
              StreamHeaderError::none);
}

TEST(StreamHeaderTest, RejectsUndefinedHashAndExtensionRegions) {
    marc::frame::StreamHeader header{};
    header.hash_descriptors_size = 1;
    EXPECT_EQ(marc::frame::validate_stream_header(
                  header, marc::core::DecoderLimits{}),
              StreamHeaderError::unsupported_feature);

    header.hash_descriptors_size = 0;
    header.header_extension_size = 1;
    EXPECT_EQ(marc::frame::validate_stream_header(
                  header, marc::core::DecoderLimits{}),
              StreamHeaderError::unsupported_feature);
}

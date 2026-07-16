#include "core/header_accumulator.hpp"
#include "core/crc32c.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

class FrameHeaderTest : public testing::Test {
protected:
    FrameHeaderTest() {
        stream_.original_size = 3;
    }

    [[nodiscard]] marc::frame::FrameValidationContext context(
        const std::uint64_t sequence = 0,
        const std::uint64_t committed = 0) const {
        return {stream_, limits_, sequence, committed};
    }

    [[nodiscard]] marc::frame::FrameValidationContext hash_context(
        const std::uint64_t sequence = 0,
        const std::uint64_t committed = 0) const {
        return {stream_, limits_, sequence, committed,
                std::span{&checksum_descriptor_, 1}};
    }

    void select_hash_profile() {
        stream_.minor_version = marc::frame::hash_format_minor_version;
        stream_.hash_descriptors_size = 16;
    }

    [[nodiscard]] static marc::frame::FrameHeader raw_header() {
        marc::frame::FrameHeader header{};
        header.uncompressed_size = 3;
        header.dictionary_serialized_size = 3;
        header.compressed_payload_size = 3;
        return header;
    }

    [[nodiscard]] static std::array<std::byte, marc::frame::frame_header_size>
    raw_vector() {
        std::array<std::byte, marc::frame::frame_header_size> bytes{};
        bytes[0] = std::byte{0x4d};
        bytes[1] = std::byte{0x52};
        bytes[2] = std::byte{0x46};
        bytes[3] = std::byte{0x31};
        bytes[4] = std::byte{0x38};
        bytes[16] = std::byte{0x03};
        bytes[20] = std::byte{0x03};
        bytes[24] = std::byte{0x03};
        return bytes;
    }

    [[nodiscard]] static marc::frame::FrameHeader hash_raw_header() {
        auto header = raw_header();
        header.checksum_trailer_size = 4;
        return header;
    }

    [[nodiscard]] static std::array<std::byte, marc::frame::frame_header_size>
    hash_raw_vector() {
        auto bytes = raw_vector();
        bytes[36] = std::byte{0x04};
        return bytes;
    }

    marc::frame::StreamHeader stream_{};
    marc::core::DecoderLimits limits_{};
    marc::frame::HashDescriptor checksum_descriptor_{
        marc::core::crc32c_algorithm_id,
        marc::frame::HashTarget::uncompressed_bytes,
        marc::frame::HashScope::per_frame,
        4,
        0};
};

} // namespace

using marc::frame::FrameHeaderError;

TEST_F(FrameHeaderTest, SerializesCanonicalRawFrame) {
    std::array<std::byte, marc::frame::frame_header_size> output{};
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  raw_header(), context(), output),
              FrameHeaderError::none);
    EXPECT_EQ(output, raw_vector());
}

TEST_F(FrameHeaderTest, ParsesCanonicalRawFrame) {
    marc::frame::FrameHeader header;
    ASSERT_EQ(marc::frame::parse_frame_header(
                  raw_vector(), context(), header),
              FrameHeaderError::none);
    EXPECT_EQ(header.sequence, 0U);
    EXPECT_EQ(header.uncompressed_size, 3U);
    EXPECT_EQ(header.dictionary_serialized_size, 3U);
    EXPECT_EQ(header.compressed_payload_size, 3U);
}

TEST_F(FrameHeaderTest, ParsesAfterEveryInputSplit) {
    const auto input = raw_vector();
    for (std::size_t split = 0; split <= input.size(); ++split) {
        marc::core::HeaderAccumulator<marc::frame::frame_header_size>
            accumulator;
        (void)accumulator.append(
            std::span<const std::byte>{input}.first(split));
        (void)accumulator.append(
            std::span<const std::byte>{input}.subspan(split));
        ASSERT_TRUE(accumulator.bytes().has_value()) << "split=" << split;
        marc::frame::FrameHeader header;
        EXPECT_EQ(marc::frame::parse_frame_header(
                      *accumulator.bytes(), context(), header),
                  FrameHeaderError::none)
            << "split=" << split;
    }
}

TEST_F(FrameHeaderTest, RejectsMalformedFixedFields) {
    marc::frame::FrameHeader header;
    auto input = raw_vector();
    input[0] = std::byte{0};
    EXPECT_EQ(marc::frame::parse_frame_header(input, context(), header),
              FrameHeaderError::invalid_magic);

    input = raw_vector();
    input[4] = std::byte{0x37};
    EXPECT_EQ(marc::frame::parse_frame_header(input, context(), header),
              FrameHeaderError::invalid_header_size);

    input = raw_vector();
    input[55] = std::byte{1};
    EXPECT_EQ(marc::frame::parse_frame_header(input, context(), header),
              FrameHeaderError::nonzero_reserved);
}

TEST_F(FrameHeaderTest, RejectsUnexpectedSequence) {
    auto header = raw_header();
    header.sequence = 1;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::unexpected_sequence);
}

TEST_F(FrameHeaderTest, RequiresFullNonFinalAndExactFinalFrame) {
    stream_.frame_size = 4;
    stream_.original_size = 6;

    auto first = raw_header();
    first.uncompressed_size = 4;
    first.dictionary_serialized_size = 4;
    first.compressed_payload_size = 4;
    EXPECT_EQ(marc::frame::validate_frame_header(first, context()),
              FrameHeaderError::none);

    first.uncompressed_size = 3;
    first.dictionary_serialized_size = 3;
    first.compressed_payload_size = 3;
    EXPECT_EQ(marc::frame::validate_frame_header(first, context()),
              FrameHeaderError::unexpected_frame_size);

    auto final = raw_header();
    final.sequence = 1;
    final.uncompressed_size = 2;
    final.dictionary_serialized_size = 2;
    final.compressed_payload_size = 2;
    EXPECT_EQ(marc::frame::validate_frame_header(final, context(1, 4)),
              FrameHeaderError::none);
}

TEST_F(FrameHeaderTest, RejectsFrameAfterDeclaredOutput) {
    EXPECT_EQ(marc::frame::validate_frame_header(raw_header(), context(1, 3)),
              FrameHeaderError::unexpected_sequence);

    auto header = raw_header();
    header.sequence = 1;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context(1, 3)),
              FrameHeaderError::unexpected_frame_size);
}

TEST_F(FrameHeaderTest, RawPipelineRequiresEqualLayerSizes) {
    auto header = raw_header();
    header.dictionary_serialized_size = 2;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::contradictory_sizes);

    header = raw_header();
    header.compressed_payload_size = 2;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::contradictory_sizes);
}

TEST_F(FrameHeaderTest, BlockBufferedEntropyRequiresDescriptors) {
    stream_.entropy_algorithm = marc::frame::EntropyAlgorithm::blocked_huffman;
    stream_.entropy_variant = 1;
    stream_.entropy_block_size = 65536;

    auto header = raw_header();
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::contradictory_sizes);

    header.entropy_block_count = 1;
    header.block_descriptors_size = 16;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::none);
}

TEST_F(FrameHeaderTest, RejectsUndefinedFlagsAndChecksumTrailer) {
    auto header = raw_header();
    header.flags = 1;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::unknown_flags);

    header.flags = 0;
    header.checksum_trailer_size = 1;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::unsupported_feature);
}

TEST_F(FrameHeaderTest, RejectsOversizedDictionarySerialization) {
    auto header = raw_header();
    stream_.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lz77;
    stream_.dictionary_variant = 1;
    limits_.max_dictionary_serialized_size = 2;
    EXPECT_EQ(marc::frame::validate_frame_header(header, context()),
              FrameHeaderError::limit_exceeded);
}

TEST_F(FrameHeaderTest, StagedV11SerializesAndParsesChecksumHeader) {
    select_hash_profile();
    std::array<std::byte, marc::frame::frame_header_size> output{};
    ASSERT_EQ(marc::frame::serialize_frame_header_v1_1(
                  hash_raw_header(), hash_context(), output),
              FrameHeaderError::none);
    EXPECT_EQ(output, hash_raw_vector());

    marc::frame::FrameHeader parsed{};
    ASSERT_EQ(marc::frame::parse_frame_header_v1_1(
                  output, hash_context(), parsed),
              FrameHeaderError::none);
    EXPECT_EQ(parsed.checksum_trailer_size, 4U);
    EXPECT_EQ(parsed.uncompressed_size, 3U);
}

TEST_F(FrameHeaderTest, VersionSpecificFrameEntryPointsRemainIsolated) {
    select_hash_profile();
    EXPECT_EQ(marc::frame::validate_frame_header(
                  hash_raw_header(), hash_context()),
              FrameHeaderError::unsupported_stream_version);

    stream_.minor_version = marc::frame::format_minor_version;
    stream_.hash_descriptors_size = 0;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  raw_header(), context()),
              FrameHeaderError::unsupported_stream_version);

    EXPECT_EQ(marc::frame::validate_frame_header(
                  raw_header(), hash_context()),
              FrameHeaderError::unsupported_feature);
}

TEST_F(FrameHeaderTest, StagedV11RequiresThreeWayChecksumAgreement) {
    select_hash_profile();
    auto header = hash_raw_header();
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, context()),
              FrameHeaderError::invalid_checksum_profile);

    stream_.hash_descriptors_size = 32;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, hash_context()),
              FrameHeaderError::invalid_checksum_profile);
    stream_.hash_descriptors_size = 16;

    auto unsupported = checksum_descriptor_;
    unsupported.scope = marc::frame::HashScope::whole_stream;
    const marc::frame::FrameValidationContext unsupported_context{
        stream_, limits_, 0, 0, std::span{&unsupported, 1}};
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, unsupported_context),
              FrameHeaderError::invalid_checksum_profile);

    header.checksum_trailer_size = 0;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, hash_context()),
              FrameHeaderError::invalid_checksum_profile);
    header.checksum_trailer_size = 5;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, hash_context()),
              FrameHeaderError::invalid_checksum_profile);
}

TEST_F(FrameHeaderTest, StagedV11CountsTrailerAgainstLocalBufferLimit) {
    select_hash_profile();
    stream_.entropy_algorithm = marc::frame::EntropyAlgorithm::adaptive_huffman;
    stream_.entropy_variant = 1;
    auto header = hash_raw_header();
    header.entropy_block_count = 1;
    header.block_descriptors_size = 16;

    limits_.max_internal_buffered_bytes = 19;
    limits_.max_block_size = 19;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, hash_context()),
              FrameHeaderError::limit_exceeded);

    limits_.max_internal_buffered_bytes = 23;
    limits_.max_block_size = 23;
    EXPECT_EQ(marc::frame::validate_frame_header_v1_1(
                  header, hash_context()),
              FrameHeaderError::none);
}

TEST_F(FrameHeaderTest, StagedV11FailuresAreTransactional) {
    select_hash_profile();
    auto malformed = hash_raw_vector();
    malformed[36] = std::byte{0x05};
    marc::frame::FrameHeader parsed{};
    parsed.sequence = 123;
    EXPECT_EQ(marc::frame::parse_frame_header_v1_1(
                  malformed, hash_context(), parsed),
              FrameHeaderError::invalid_checksum_profile);
    EXPECT_EQ(parsed.sequence, 123U);

    std::array<std::byte, marc::frame::frame_header_size> output{};
    output.fill(std::byte{0xa5});
    const auto unchanged = output;
    auto invalid = hash_raw_header();
    invalid.checksum_trailer_size = 5;
    EXPECT_EQ(marc::frame::serialize_frame_header_v1_1(
                  invalid, hash_context(), output),
              FrameHeaderError::invalid_checksum_profile);
    EXPECT_EQ(output, unchanged);
}

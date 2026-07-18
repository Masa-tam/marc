#include "frame/lzmw_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzmw_blocked_huffman_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame;

constexpr std::array raw{std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
                         std::byte{'B'}, std::byte{'X'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = raw.size();
    stream.entropy_block_size = 4;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

marc::dictionary::internal::LzmwParameters parameters() {
    return {static_cast<std::uint32_t>(raw.size() - 1), 0, 0};
}

marc::core::DecoderLimits limits() {
    auto result = marc::core::DecoderLimits{};
    result.max_total_output_size = raw.size();
    result.max_frame_size = raw.size();
    result.max_block_size = 4;
    result.max_compressed_payload_size = 20;
    result.max_dictionary_serialized_size = 20;
    result.max_internal_buffered_bytes = 512;
    result.max_dictionary_entries = raw.size() - 1;
    result.max_blocks_per_frame = 5;
    return result;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    std::vector<std::byte> encoded(lzmw_blocked_huffman_stream_prefix_size);
    const std::span<std::byte, stream_header_size> header{
        encoded.data(), stream_header_size};
    const std::span<std::byte,
                    marc::dictionary::internal::lzmw_parameter_size>
        parameter_bytes{encoded.data() + stream_header_size,
                        marc::dictionary::internal::lzmw_parameter_size};
    EXPECT_EQ(serialize_stream_header(stream, {}, header),
              StreamHeaderError::none);
    EXPECT_EQ(marc::dictionary::internal::serialize_lzmw_parameters(
                  parameters(), limits(), parameter_bytes),
              marc::dictionary::internal::LzmwFormatError::none);

    std::array<marc::dictionary::internal::LzmwEncoderEntry, raw.size() - 1>
        entries{};
    std::array<std::byte, raw.size() * 4> staging{};
    const auto plan = plan_lzmw_blocked_huffman_frame(
        stream, parameters(), limits(), 0, 0, raw, entries, staging);
    EXPECT_EQ(plan.error, LzmwBlockedHuffmanFrameValidationError::none);
    const auto offset = encoded.size();
    encoded.resize(offset + plan.serialized_size);
    EXPECT_EQ(encode_lzmw_blocked_huffman_frame(
                  stream, parameters(), limits(), 0, 0, raw, entries, staging,
                  std::span<std::byte>{encoded}.subspan(offset))
                  .error,
              LzmwBlockedHuffmanFrameValidationError::none);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<std::byte, 256> encoded_frame{};
    std::array<std::byte, 20> dictionary{};
    std::array<std::byte, raw.size()> decoded_frame{};
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 5> views{};
    std::array<marc::dictionary::internal::LzmwPhraseEntry, raw.size() - 1>
        phrases{};
    std::array<std::uint32_t, raw.size()> expansion{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    LzmwBlockedHuffmanFrameStreamingDecoder decoder{
        limits(), encoded_frame, dictionary, decoded_frame, views, phrases,
        expansion};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    const auto repeated = decoder.process(
        {}, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(repeated.status, result.status);
    EXPECT_EQ(repeated.error.code, result.error.code);
    EXPECT_EQ(repeated.input_consumed, 0U);
    EXPECT_EQ(repeated.output_produced, 0U);
}

std::uint32_t load_le32(const std::span<const std::byte> input,
                        const std::size_t offset) {
    return std::to_integer<std::uint32_t>(input[offset])
        | std::to_integer<std::uint32_t>(input[offset + 1]) << 8
        | std::to_integer<std::uint32_t>(input[offset + 2]) << 16
        | std::to_integer<std::uint32_t>(input[offset + 3]) << 24;
}

TEST(LzmwBlockedHuffmanFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size)
        expect_atomic_failure(std::span<const std::byte>{encoded}.first(size));
}

TEST(LzmwBlockedHuffmanFuzzRegression, ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream();
    std::fill(encoded.begin() + lzmw_blocked_huffman_stream_prefix_size + 16,
              encoded.begin() + lzmw_blocked_huffman_stream_prefix_size + 40,
              std::byte{0xff});
    expect_atomic_failure(encoded);
}

TEST(LzmwBlockedHuffmanFuzzRegression,
     UnavailableStagedReferenceIsAtomic) {
    auto encoded = canonical_stream();
    constexpr auto frame = lzmw_blocked_huffman_stream_prefix_size;
    const auto descriptor_size = load_le32(encoded, frame + 32);
    const auto second_reference = frame + frame_header_size
        + descriptor_size + marc::dictionary::internal::lzmw_token_size;
    ASSERT_LE(second_reference + 4, encoded.size());
    encoded[second_reference] = std::byte{0x00};
    encoded[second_reference + 1] = std::byte{0x01};
    encoded[second_reference + 2] = std::byte{0x00};
    encoded[second_reference + 3] = std::byte{0x00};
    expect_atomic_failure(encoded);
}

} // namespace

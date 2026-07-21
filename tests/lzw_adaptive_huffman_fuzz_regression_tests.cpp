#include "frame/lzw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_adaptive_huffman_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace {
using namespace marc::frame;

using EncoderEntry = marc::dictionary::internal::LzwEncoderEntry;
using Phrase = marc::dictionary::internal::LzwPhraseEntry;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
    std::byte{'B'}, std::byte{'X'}};

[[nodiscard]] StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::adaptive_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = raw.size();
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

[[nodiscard]] std::vector<std::byte> canonical_stream() {
    std::array<std::byte, raw.size()> frame_input{};
    std::array<std::byte, raw.size() * 2> dictionary{};
    std::array<std::byte, 4096> frame_encoded{};
    std::array<EncoderEntry, raw.size() - 1> entries{};
    LzwAdaptiveHuffmanFrameStreamingEncoder encoder{
        config(), {}, {}, frame_input, dictionary, frame_encoded, entries};
    std::vector<std::byte> encoded(8192);
    const auto result = encoder.process(
        raw, encoded,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::end_of_stream);
    encoded.resize(result.output_produced);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<std::byte, 8192> encoded_frame{};
    std::array<std::byte, raw.size() * 2> dictionary{};
    std::array<std::byte, raw.size()> decoded_frame{};
    std::array<Phrase, raw.size()> phrases{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    LzwAdaptiveHuffmanFrameStreamingDecoder decoder{
        {}, encoded_frame, dictionary, decoded_frame, phrases};
    const auto result = decoder.process(
        input, output,
        marc::core::flag_value(marc::core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, marc::core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    const auto repeated = decoder.process({}, {}, 0);
    EXPECT_EQ(repeated.status, marc::core::StreamStatus::error);
    EXPECT_EQ(repeated.error.code, result.error.code);
    EXPECT_EQ(repeated.error.byte_position, result.error.byte_position);
}

TEST(LzwAdaptiveHuffmanFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_atomic_failure(std::span<const std::byte>{encoded}.first(size));
    }
}

TEST(LzwAdaptiveHuffmanFuzzRegression, ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream();
    std::fill(encoded.begin() + 80 + 16, encoded.begin() + 80 + 40,
              std::byte{0xff});
    expect_atomic_failure(encoded);
}

TEST(LzwAdaptiveHuffmanFuzzRegression, InvalidDescriptorIsAtomic) {
    auto encoded = canonical_stream();
    encoded[80 + frame_header_size
            + marc::entropy::internal::adaptive_huffman_descriptor_size - 1] =
        std::byte{1};
    expect_atomic_failure(encoded);
}

} // namespace

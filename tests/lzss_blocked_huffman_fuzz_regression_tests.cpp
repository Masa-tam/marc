#include "frame/lzss_blocked_huffman_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
    std::byte{'B'}, std::byte{'X'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
    stream.entropy_variant = 1;
    stream.frame_size = 2;
    stream.entropy_block_size = 4;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    std::array<std::byte, 10> dictionary{};
    const auto plan = plan_lzss_blocked_huffman_stream(
        stream, {}, {}, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzss_blocked_huffman_stream(
                  stream, {}, {}, raw, dictionary, encoded).error,
              LzssBlockedHuffmanStreamCodecError::none);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<marc::entropy::internal::BlockedHuffmanBlockView, 3> views{};
    std::array<std::byte, 10> dictionary{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    StreamHeader stream{};
    stream.original_size = 99;
    marc::dictionary::internal::LzssParameters parameters{};
    parameters.window_size = 17;
    const auto result = decode_lzss_blocked_huffman_stream(
        input, {}, views, dictionary, output, stream, parameters);
    EXPECT_NE(result.error, LzssBlockedHuffmanStreamCodecError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    EXPECT_EQ(stream.original_size, 99U);
    EXPECT_EQ(parameters.window_size, 17U);
}

TEST(LzssBlockedHuffmanFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_atomic_failure(
            std::span<const std::byte>{encoded}.first(size));
    }
}

TEST(LzssBlockedHuffmanFuzzRegression, ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream();
    std::fill(encoded.begin() + 80 + 16, encoded.begin() + 80 + 40,
              std::byte{0xff});
    expect_atomic_failure(encoded);
}

TEST(LzssBlockedHuffmanFuzzRegression, InvalidStagedTokenIsAtomic) {
    auto encoded = canonical_stream();
    constexpr std::size_t second_frame_payload = 80 + 76 + 56 + 16;
    ASSERT_LT(second_frame_payload, encoded.size());
    encoded[second_frame_payload] = std::byte{0x7f};
    expect_atomic_failure(encoded);
}

} // namespace

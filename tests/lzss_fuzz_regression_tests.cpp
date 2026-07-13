#include "frame/lzss_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 6;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    const auto plan = plan_lzss_stream(stream, {}, {}, raw);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzss_stream(stream, {}, {}, raw, encoded).error,
              LzssStreamCodecError::none);
    return encoded;
}

TEST(LzssFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        std::array<std::byte, raw.size()> output{};
        output.fill(std::byte{0xa5});
        StreamHeader stream{};
        marc::dictionary::internal::LzssParameters parameters{};
        const auto result = decode_lzss_stream(
            std::span<const std::byte>{encoded}.first(size), {}, output,
            stream, parameters);
        EXPECT_NE(result.error, LzssStreamCodecError::none) << size;
        EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
            return value == std::byte{0xa5};
        })) << size;
    }
}

TEST(LzssFuzzRegression, ExtremeLengthFieldsFailWithoutOutput) {
    auto encoded = canonical_stream();
    std::fill(encoded.begin() + 80 + 16, encoded.begin() + 80 + 28,
              std::byte{0xff});
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0x5a});
    StreamHeader stream{};
    marc::dictionary::internal::LzssParameters parameters{};
    const auto result = decode_lzss_stream(
        encoded, {}, output, stream, parameters);
    EXPECT_NE(result.error, LzssStreamCodecError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0x5a};
    }));
}

} // namespace

#include "frame/lzw_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzw_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    std::array<marc::dictionary::internal::LzwEncoderEntry, 2> dictionary{};
    const auto plan = plan_lzw_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzw_stream(
                  stream, {}, {}, raw, dictionary, encoded).error,
              LzwStreamCodecError::none);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<marc::dictionary::internal::LzwPhraseEntry, 1> dictionary{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    StreamHeader stream{};
    stream.original_size = 99;
    marc::dictionary::internal::LzwParameters parameters{};
    parameters.maximum_code_width = 9;
    const auto result = decode_lzw_stream(
        input, {}, dictionary, output, stream, parameters);
    EXPECT_NE(result.error, LzwStreamCodecError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    EXPECT_EQ(stream.original_size, 99U);
    EXPECT_EQ(parameters.maximum_code_width, 9U);
}

TEST(LzwFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_atomic_failure(
            std::span<const std::byte>{encoded}.first(size));
    }
}

TEST(LzwFuzzRegression, CodePaddingAndHeaderMutationsAreAtomic) {
    const auto canonical = canonical_stream();
    constexpr std::size_t first_payload = 80 + frame_header_size;

    auto first_code = canonical;
    first_code[first_payload] = std::byte{0};
    first_code[first_payload + 1] = std::byte{1};
    expect_atomic_failure(first_code);

    auto padding = canonical;
    padding[first_payload + 2] |= std::byte{0x80};
    expect_atomic_failure(padding);

    auto extreme = canonical;
    std::fill(extreme.begin() + 80 + 16, extreme.begin() + 80 + 28,
              std::byte{0xff});
    expect_atomic_failure(extreme);
}

TEST(LzwFuzzRegression, CrossFrameDictionaryReferenceIsAtomic) {
    auto encoded = canonical_stream();
    constexpr std::size_t second_payload = 139 + frame_header_size;
    encoded[second_payload] = std::byte{0};
    encoded[second_payload + 1] = std::byte{1};
    expect_atomic_failure(encoded);
}

} // namespace

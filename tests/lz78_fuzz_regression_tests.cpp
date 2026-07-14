#include "frame/lz78_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'},
    std::byte{'A'}, std::byte{'A'}, std::byte{'A'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 3;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz78_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    std::array<marc::dictionary::internal::Lz78EncoderEntry, 3> dictionary{};
    const auto plan = plan_lz78_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lz78_stream(
                  stream, {}, {}, raw, dictionary, encoded).error,
              Lz78StreamCodecError::none);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<marc::dictionary::internal::Lz78PhraseEntry, 2> dictionary{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    StreamHeader stream{};
    stream.original_size = 99;
    marc::dictionary::internal::Lz78Parameters parameters{};
    parameters.maximum_entries = 9;
    const auto result = decode_lz78_stream(
        input, {}, dictionary, output, stream, parameters);
    EXPECT_NE(result.error, Lz78StreamCodecError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    EXPECT_EQ(stream.original_size, 99U);
    EXPECT_EQ(parameters.maximum_entries, 9U);
}

TEST(Lz78FuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size) {
        expect_atomic_failure(
            std::span<const std::byte>{encoded}.first(size));
    }
}

TEST(Lz78FuzzRegression, TokenHeaderAndLengthMutationsAreAtomic) {
    const auto canonical = canonical_stream();
    constexpr std::size_t first_payload = 80 + frame_header_size;
    for (const std::pair<std::size_t, std::byte> mutation : {
             std::pair{first_payload, std::byte{2}},
             std::pair{first_payload + 2, std::byte{1}},
             std::pair{first_payload + 3, std::byte{1}},
             std::pair{first_payload + 4, std::byte{1}}}) {
        auto encoded = canonical;
        encoded[mutation.first] = mutation.second;
        expect_atomic_failure(encoded);
    }

    auto extreme = canonical;
    std::fill(extreme.begin() + 80 + 16, extreme.begin() + 80 + 28,
              std::byte{0xff});
    expect_atomic_failure(extreme);
}

TEST(Lz78FuzzRegression, CrossFramePhraseReferenceIsAtomic) {
    auto encoded = canonical_stream();
    constexpr std::size_t second_payload = 152 + frame_header_size;
    encoded[second_payload + 4] = std::byte{1};
    expect_atomic_failure(encoded);
}

} // namespace

#include "frame/lzmw_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'}, std::byte{'B'}};

StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lzmw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = 2;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzmw_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

std::vector<std::byte> canonical_stream() {
    const auto stream = config();
    std::array<marc::dictionary::internal::LzmwEncoderEntry, 1> dictionary{};
    const auto plan = plan_lzmw_stream(stream, {}, {}, raw, dictionary);
    std::vector<std::byte> encoded(plan.serialized_size);
    EXPECT_EQ(encode_lzmw_stream(
                  stream, {}, {}, raw, dictionary, encoded).error,
              LzmwStreamCodecError::none);
    return encoded;
}

void expect_atomic_failure(const std::span<const std::byte> input) {
    std::array<marc::dictionary::internal::LzmwPhraseEntry, 1> phrases{};
    std::array<std::uint32_t, 2> expansion{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    StreamHeader stream{};
    stream.original_size = 99;
    marc::dictionary::internal::LzmwParameters parameters{};
    parameters.maximum_entries = 9;
    const auto result = decode_lzmw_stream(
        input, {}, phrases, expansion, output, stream, parameters);
    EXPECT_NE(result.error, LzmwStreamCodecError::none);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
    EXPECT_EQ(stream.original_size, 99U);
    EXPECT_EQ(parameters.maximum_entries, 9U);
}

TEST(LzmwFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size)
        expect_atomic_failure(std::span<const std::byte>{encoded}.first(size));
}

TEST(LzmwFuzzRegression, TokenExtentAndHeaderMutationsAreAtomic) {
    const auto canonical = canonical_stream();
    constexpr std::size_t first_frame = lzmw_stream_prefix_size;
    constexpr std::size_t first_payload = first_frame + frame_header_size;

    auto absent_first = canonical;
    std::fill(absent_first.begin() + first_payload,
              absent_first.begin() + first_payload + 4, std::byte{0xff});
    expect_atomic_failure(absent_first);

    auto absent_second = canonical;
    std::fill(absent_second.begin() + first_payload + 4,
              absent_second.begin() + first_payload + 8, std::byte{0xff});
    expect_atomic_failure(absent_second);

    auto forward = canonical;
    forward[first_payload] = std::byte{0};
    forward[first_payload + 1] = std::byte{1};
    expect_atomic_failure(forward);

    auto extent = canonical;
    extent[first_frame + 20] = std::byte{7};
    extent[first_frame + 24] = std::byte{7};
    expect_atomic_failure(extent);

    auto extreme = canonical;
    std::fill(extreme.begin() + first_frame + 16,
              extreme.begin() + first_frame + 28, std::byte{0xff});
    expect_atomic_failure(extreme);
}

TEST(LzmwFuzzRegression, CrossFramePhraseReferenceIsAtomic) {
    auto encoded = canonical_stream();
    constexpr std::size_t second_payload = 144 + frame_header_size;
    encoded[second_payload] = std::byte{0};
    encoded[second_payload + 1] = std::byte{1};
    expect_atomic_failure(encoded);
}

} // namespace

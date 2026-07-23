#include "frame/lz77_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lz77_dynamic_range_frame_streaming_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace {
using namespace marc::frame;

constexpr std::array raw{
    std::byte{'A'}, std::byte{'B'}, std::byte{'A'},
    std::byte{'B'}, std::byte{'X'}};

[[nodiscard]] StreamHeader config() {
    StreamHeader stream{};
    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = raw.size();
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lz77_parameter_size;
    stream.original_size = raw.size();
    return stream;
}

[[nodiscard]] std::vector<std::byte> canonical_stream() {
    std::array<std::byte, raw.size()> frame_input{};
    std::array<std::byte, raw.size() * 16> dictionary{};
    std::array<std::byte, 4096> frame_encoded{};
    Lz77DynamicRangeFrameStreamingEncoder encoder{
        config(), {}, {}, frame_input, dictionary, frame_encoded};
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
    std::array<std::byte, raw.size() * 16> dictionary{};
    std::array<std::byte, raw.size()> decoded_frame{};
    std::array<std::byte, raw.size()> output{};
    output.fill(std::byte{0xa5});
    Lz77DynamicRangeFrameStreamingDecoder decoder{
        {}, encoded_frame, dictionary, decoded_frame};
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

TEST(Lz77DynamicRangeFuzzRegression, EveryCanonicalTruncationIsAtomic) {
    const auto encoded = canonical_stream();
    for (std::size_t size = 0; size < encoded.size(); ++size)
        expect_atomic_failure(std::span<const std::byte>{encoded}.first(size));
}

TEST(Lz77DynamicRangeFuzzRegression, ExtremeFrameLengthsAreAtomic) {
    auto encoded = canonical_stream();
    const auto frame_offset = lz77_dynamic_range_stream_prefix_size;
    std::fill(encoded.begin() + frame_offset + 16,
              encoded.begin() + frame_offset + 40,
              std::byte{0xff});
    expect_atomic_failure(encoded);
}

TEST(Lz77DynamicRangeFuzzRegression, InvalidDescriptorIsAtomic) {
    auto encoded = canonical_stream();
    encoded[lz77_dynamic_range_stream_prefix_size + frame_header_size
            + marc::entropy::internal::dynamic_range_descriptor_size - 1] =
        std::byte{1};
    expect_atomic_failure(encoded);
}

} // namespace

#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_format.hpp"
#include "entropy/dynamic_range_encoder.hpp"
#include "entropy/dynamic_range_format.hpp"
#include "frame/frame_header.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace {

constexpr std::array raw_a{std::byte{0x41}};
constexpr std::array lzss_literal_a{
    std::byte{0x00}, std::byte{0x41}};
constexpr std::array dynamic_range_payload{
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0xbe},
    std::byte{0x41}, std::byte{0x7c}, std::byte{0x00}};

constexpr std::array<std::byte, 79> complete_frame{
    std::byte{0x4d}, std::byte{0x52}, std::byte{0x46}, std::byte{0x31},
    std::byte{0x38}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0xbe},
    std::byte{0x41}, std::byte{0x7c}, std::byte{0x00}};

[[nodiscard]] marc::frame::StreamHeader stream_for_a() {
    marc::frame::StreamHeader stream{};
    stream.dictionary_algorithm = marc::frame::DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = marc::frame::EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = 1;
    stream.dictionary_parameters_size =
        marc::dictionary::internal::lzss_parameter_size;
    stream.original_size = 1;
    return stream;
}

TEST(LzssDynamicRangeVector, EmitsIndependentSingleLiteralFrame) {
    std::array<std::byte, lzss_literal_a.size()> tokens{};
    const auto dictionary =
        marc::dictionary::internal::encode_lzss_token_stream(
            raw_a, {}, {}, tokens);
    ASSERT_EQ(dictionary.error,
              marc::dictionary::internal::LzssEncodeError::none);
    ASSERT_EQ(tokens, lzss_literal_a);

    marc::entropy::internal::DynamicRangeDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_dynamic_range_frame(
        tokens, {}, descriptor);
    ASSERT_EQ(plan.error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    EXPECT_EQ(plan.payload_size, dynamic_range_payload.size());

    std::array<std::byte, dynamic_range_payload.size()> payload{};
    ASSERT_EQ(marc::entropy::internal::encode_dynamic_range_frame(
                  tokens, {}, payload, descriptor).error,
              marc::entropy::internal::DynamicRangeEncodeError::none);
    EXPECT_EQ(payload, dynamic_range_payload);
    EXPECT_EQ(descriptor.symbol_count, lzss_literal_a.size());
    EXPECT_EQ(descriptor.payload_size, dynamic_range_payload.size());

    std::array<std::byte, complete_frame.size()> frame{};
    marc::frame::FrameHeader header{};
    header.uncompressed_size = 1;
    header.dictionary_serialized_size = lzss_literal_a.size();
    header.compressed_payload_size = dynamic_range_payload.size();
    header.entropy_block_count = 1;
    header.block_descriptors_size =
        marc::entropy::internal::dynamic_range_descriptor_size;
    const marc::core::DecoderLimits limits{};
    const auto stream = stream_for_a();
    ASSERT_EQ(marc::frame::serialize_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, marc::frame::frame_header_size>{
                      frame.data(), marc::frame::frame_header_size}),
              marc::frame::FrameHeaderError::none);

    ASSERT_EQ(marc::entropy::internal::serialize_dynamic_range_descriptor(
                  descriptor, lzss_literal_a.size(),
                  dynamic_range_payload.size(), limits,
                  std::span<std::byte,
                            marc::entropy::internal::
                                dynamic_range_descriptor_size>{
                      frame.data() + marc::frame::frame_header_size,
                      marc::entropy::internal::
                          dynamic_range_descriptor_size}),
              marc::entropy::internal::DynamicRangeFormatError::none);
    std::ranges::copy(
        payload,
        frame.begin() + marc::frame::frame_header_size
            + marc::entropy::internal::dynamic_range_descriptor_size);
    EXPECT_EQ(frame, complete_frame);
}

} // namespace
